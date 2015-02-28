/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * NOTE: UI renderer is built under assumption that UI components need not use attributes.
 *       This is owing to the fact ogl_ui uses the context-wide no-VAA VAO for rendering.
 *       Change this if necessary (but face the performance penalty due to VAO rebindings).
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_button.h"
#include "ogl/ogl_ui_checkbox.h"
#include "ogl/ogl_ui_dropdown.h"
#include "ogl/ogl_ui_frame.h"
#include "ogl/ogl_ui_label.h"
#include "ogl/ogl_ui_label.h"
#include "ogl/ogl_ui_scrollbar.h"
#include "ogl/ogl_ui_texture_preview.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"

/** Internal definitions */
#define N_START_CONTROLS     (4)

/** Internal types */
typedef void (*PFNOGLUIDEINITPROCPTR)        (      void*  internal_instance);
typedef void (*PFNOGLUIDRAWPROCPTR)          (      void*  internal_instance);
typedef void (*PFNOGLUIGETPROPERTYPROCPTR)   (const void*  internal_instance,
                                                    int    property_value,
                                                    void*  out_result);
typedef void (*PFNOGLUIHOVERPROCPTR)         (      void*  internal_instance,
                                              const float* xy_screen_norm);
typedef bool (*PFNOGLUIISOVERPROCPTR)        (      void*  internal_instance,
                                              const float* xy);
typedef void (*PFNOGLUIONLBMDOWNPROCPTR)     (      void*  internal_instance,
                                              const float* xy);
typedef void (*PFNOGLUIONLBMUPPROCPTR)       (      void*  internal_instance,
                                              const float* xy);
typedef void (*PFNOGLUIONMOUSEMOVEPROCPTR)   (      void*  internal_instance,
                                              const float* xy);
typedef void (*PFNOGLUIONMOUSEWHEELPROCPTR)  (      void*  internal_instance,
                                              float        wheel_delta);
typedef void (*PFNOGLUISETPROPERTYPROCPTR)   (      void*  internal_instance,
                                              int          property_value,
                                              const void*  data);

typedef struct _ogl_ui_callback
{
    int                          callback_id;
    PFNOGLUIEVENTCALLBACKPROCPTR callback_proc_ptr;
    void*                        callback_proc_user_arg;

    _ogl_ui_callback()
    {
        callback_id            = 0;
        callback_proc_ptr      = NULL;
        callback_proc_user_arg = NULL;
    }
} _ogl_ui_callback;

typedef struct
{
    float xy[2];
} _ogl_ui_click;

typedef struct
{
    system_resizable_vector   controls;
    system_hashed_ansi_string name;
    system_critical_section   rendering_cs;
    ogl_text                  text_renderer;
    system_window             window;

    system_hash64map registered_ui_control_callbacks; /* stores a system_resizable_vector storing _ogl_ui_callback instances.
                                                       * this is the least optimal way of storing registered callbacks,
                                                       * but given the fact we're not expecting a lot of those, we should be fine.
                                                       */
    system_hash64map registered_ui_control_programs;

    bool  current_lbm_status;
    float current_mouse_xy[2];

    /* Cached GL func ptrs */
    PFNGLDISABLEPROC         pGLDisable;
    PFNGLBINDVERTEXARRAYPROC pGLBindVertexArray;

    REFCOUNT_INSERT_VARIABLES
} _ogl_ui;

typedef struct
{
    /* NOT called from a rendering thread */
    PFNOGLUIDEINITPROCPTR         pfn_deinit_func_ptr;
    /* CALLED from a rendering thread */
    PFNOGLUIDRAWPROCPTR           pfn_draw_func_ptr;
    /* NOT called from a rendering thread */
    PFNOGLUIGETPROPERTYPROCPTR    pfn_get_property_func_ptr;
    /* NOT called from a rendering thread */
    PFNOGLUIISOVERPROCPTR         pfn_is_over_func_ptr;
    /* NOT called from a rendering thread */
    PFNOGLUIONLBMDOWNPROCPTR      pfn_on_lbm_down_func_ptr;
    /* NOT called from a rendering thread */
    PFNOGLUIONLBMUPPROCPTR        pfn_on_lbm_up_func_ptr;
    /* NOT called from a rendering thread */
    PFNOGLUIONMOUSEMOVEPROCPTR    pfn_on_mouse_move_func_ptr;
    /* NOT called from a rendering thread */
    PFNOGLUIONMOUSEWHEELPROCPTR   pfn_on_mouse_wheel_func_ptr;
    /* NOT called from a renderingt thread */
    PFNOGLUISETPROPERTYPROCPTR    pfn_set_property_func_ptr;
    void* internal;
} _ogl_ui_control;

/* Forward declarations */
PRIVATE void _ogl_ui_control_deinit           (__in __notnull                   _ogl_ui_control*);
PRIVATE void _ogl_ui_control_init             (__in __notnull                   _ogl_ui_control*);
PRIVATE void _ogl_ui_deinit                   (__in __notnull                   _ogl_ui*);
PRIVATE void _ogl_ui_init                     (__in __notnull                   _ogl_ui*,
                                               __in __notnull                   system_hashed_ansi_string,
                                               __in __notnull                   ogl_pipeline);
PRIVATE bool _ogl_ui_callback_on_lbm_down     (                                 system_window,
                                                                                LONG,
                                                                                LONG,
                                                                                system_window_vk_status,
                                                                                void*);
PRIVATE bool _ogl_ui_callback_on_lbm_up       (                                 system_window,
                                                                                LONG,
                                                                                LONG,
                                                                                system_window_vk_status,
                                                                                void*);
PRIVATE bool _ogl_ui_callback_on_mouse_move   (                                 system_window,
                                                                                LONG,
                                                                                LONG,
                                                                                system_window_vk_status,
                                                                                void*);
PRIVATE bool _ogl_ui_callback_on_mouse_wheel  (                                 system_window,
                                                                                unsigned short,
                                                                                unsigned short,
                                                                                short,
                                                                                system_window_vk_status,
                                                                                void*);
PRIVATE void* _ogl_ui_get_internal_control_ptr(__in __notnull                   ogl_ui_control);
PRIVATE void  _ogl_ui_release                 (__in __notnull __deallocate(mem) void*);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_ui,
                               ogl_ui,
                              _ogl_ui);

/** TODO */
PRIVATE void _ogl_ui_control_deinit(__in __notnull _ogl_ui_control* ui_control_ptr)
{
    if (ui_control_ptr->pfn_deinit_func_ptr != NULL)
    {
        ui_control_ptr->pfn_deinit_func_ptr(ui_control_ptr->internal);

        ui_control_ptr->internal = NULL;
    } /* if (ui_control_ptr->pfn_deinit_func_ptr != NULL) */
}

/** TODO */
PRIVATE void* _ogl_ui_get_internal_control_ptr(__in __notnull ogl_ui_control control)
{
    _ogl_ui_control* control_ptr = (_ogl_ui_control*) control;

    return control_ptr->internal;
}

/** TODO */
PRIVATE void _ogl_ui_control_init(__in __notnull _ogl_ui_control* ui_control_ptr)
{
    memset(ui_control_ptr,
           0,
           sizeof(_ogl_ui_control) );
}

/** TODO */
PRIVATE void _ogl_ui_deinit_gl_renderer_callback(ogl_context context,
                                                 void*       user_arg)
{
    ogl_context_type            context_type          = OGL_CONTEXT_TYPE_UNDEFINED;
    PFNGLDELETEVERTEXARRAYSPROC pGLDeleteVertexArrays = NULL;
    _ogl_ui*                    ui_ptr                = (_ogl_ui*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        pGLDeleteVertexArrays = entry_points->pGLDeleteVertexArrays;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unsupported context type");

        const ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        pGLDeleteVertexArrays = entry_points->pGLDeleteVertexArrays;
    }
}

/** TODO */
PRIVATE void _ogl_ui_deinit(__in __notnull _ogl_ui* ui_ptr)
{
    _ogl_ui_control* ui_control_ptr = NULL;

    /* Release call-backs */
    system_window_delete_callback_func(ui_ptr->window,
                                       SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                       _ogl_ui_callback_on_lbm_down,
                                       ui_ptr);
    system_window_delete_callback_func(ui_ptr->window,
                                       SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                       _ogl_ui_callback_on_lbm_up,
                                       ui_ptr);
    system_window_delete_callback_func(ui_ptr->window,
                                       SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                       _ogl_ui_callback_on_mouse_move,
                                       ui_ptr);

    /* Release GL stuff */
    ogl_context_request_callback_from_context_thread(ogl_text_get_context(ui_ptr->text_renderer),
                                                     _ogl_ui_deinit_gl_renderer_callback,
                                                     ui_ptr);

    /* Release all controls */
    while (system_resizable_vector_get_amount_of_elements(ui_ptr->controls) > 0)
    {
        if (!system_resizable_vector_get_element_at(ui_ptr->controls,
                                                    0,
                                                   &ui_control_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve UI control descriptor");
        }
        else
        {
            _ogl_ui_control_deinit(ui_control_ptr);

            delete ui_control_ptr;
            ui_control_ptr = NULL;

            system_resizable_vector_delete_element_at(ui_ptr->controls,
                                                      0);
        }
    }

    system_resizable_vector_release(ui_ptr->controls);
    ui_ptr->controls = NULL;

    /* Release other owned objects */
    system_critical_section_release(ui_ptr->rendering_cs);

    if (ui_ptr->text_renderer != NULL)
    {
        ogl_text_release(ui_ptr->text_renderer);

        ui_ptr->text_renderer = NULL;
    }

    if (ui_ptr->registered_ui_control_callbacks != NULL)
    {
        while (system_hash64map_get_amount_of_elements(ui_ptr->registered_ui_control_callbacks) > 0)
        {
            bool              result           = false;
            system_hash64     ui_callback_hash = 0;
            _ogl_ui_callback* ui_callback_ptr  = NULL;

            if (system_hash64map_get_element_at(ui_ptr->registered_ui_control_callbacks,
                                                0,
                                               &ui_callback_ptr,
                                               &ui_callback_hash) )
            {
                delete ui_callback_ptr;
                ui_callback_ptr = NULL;

                result = system_hash64map_remove(ui_ptr->registered_ui_control_callbacks,
                                                 ui_callback_hash);

                ASSERT_ALWAYS_SYNC(result,
                                   "Could not remove UI callback descriptor");
            }
        }

        system_hash64map_release(ui_ptr->registered_ui_control_callbacks);

        ui_ptr->registered_ui_control_callbacks = NULL;
    } /* if (ui_ptr->registered_ui_control_callbacks != NULL) */

    if (ui_ptr->registered_ui_control_programs != NULL)
    {
        while (system_hash64map_get_amount_of_elements(ui_ptr->registered_ui_control_programs) > 0)
        {
            bool          result                       = false;
            ogl_program   ui_control_program           = NULL;
            system_hash64 ui_control_program_name_hash = 0;

            if (system_hash64map_get_element_at(ui_ptr->registered_ui_control_programs,
                                                0,
                                               &ui_control_program,
                                               &ui_control_program_name_hash) )
            {
                ogl_program_release(ui_control_program);

                result = system_hash64map_remove(ui_ptr->registered_ui_control_programs,
                                                 ui_control_program_name_hash);

                ASSERT_ALWAYS_SYNC(result,
                                   "Could not remove UI control descriptor");
            } /* if (system_hash64map_get_element_at(ui_ptr->registered_ui_control_programs, 0, &ui_control_ptr, &ui_control_name_hash) ) */
        }

        system_hash64map_release(ui_ptr->registered_ui_control_programs);

        ui_ptr->registered_ui_control_programs = NULL;
    } /* if (ui_ptr->registered_ui_control_programs != NULL) */
}

/** TODO */
PRIVATE void _ogl_ui_init_gl_renderer_callback(ogl_context context,
                                               void*       user_arg)
{
    ogl_context_type         context_type       = OGL_CONTEXT_TYPE_UNDEFINED;
    PFNGLGENVERTEXARRAYSPROC pGLGenVertexArrays = NULL;
    _ogl_ui*                 ui_ptr             = (_ogl_ui*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        pGLGenVertexArrays = entry_points->pGLGenVertexArrays;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unsupported context type");

        const ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        pGLGenVertexArrays = entry_points->pGLGenVertexArrays;
    }
}

/** TODO */
PRIVATE void _ogl_ui_init(__in __notnull _ogl_ui*                  ui_ptr,
                          __in __notnull system_hashed_ansi_string name,
                          __in __notnull ogl_text                  text_renderer)
{
    ogl_context context = ogl_text_get_context(text_renderer);

    ui_ptr->controls                        = system_resizable_vector_create(N_START_CONTROLS,
                                                                             sizeof(_ogl_ui_control*) );
    ui_ptr->current_lbm_status              = false;
    ui_ptr->current_mouse_xy[0]             = 0;
    ui_ptr->current_mouse_xy[1]             = 0;
    ui_ptr->name                            = name;
    ui_ptr->registered_ui_control_callbacks = system_hash64map_create(sizeof(void*) );
    ui_ptr->registered_ui_control_programs  = system_hash64map_create(sizeof(void*) );
    ui_ptr->rendering_cs                    = system_critical_section_create();
    ui_ptr->text_renderer                   = text_renderer;
    ui_ptr->window                          = NULL;

    ogl_text_retain(text_renderer);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &ui_ptr->window);

    /* Cache GL func ptrs that will be used by the draw routine */
    ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        ui_ptr->pGLBindVertexArray = entry_points->pGLBindVertexArray;
        ui_ptr->pGLDisable         = entry_points->pGLDisable;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type");

        ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        ui_ptr->pGLBindVertexArray = entry_points->pGLBindVertexArray;
        ui_ptr->pGLDisable         = entry_points->pGLDisable;
    }

    /* Sign up for mouse events */
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                    _ogl_ui_callback_on_lbm_down,
                                    ui_ptr);
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                    _ogl_ui_callback_on_lbm_up,
                                    ui_ptr);
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                    _ogl_ui_callback_on_mouse_move,
                                    ui_ptr);
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL,
                                    _ogl_ui_callback_on_mouse_wheel,
                                    ui_ptr);

    /* Create GL-specific objects */
    ogl_context_request_callback_from_context_thread(context,
                                                     _ogl_ui_init_gl_renderer_callback,
                                                     ui_ptr);
}

/** TODO */
PRIVATE bool _ogl_ui_callback_on_lbm_down(system_window,
                                          LONG                    x,
                                          LONG                    y,
                                          system_window_vk_status vk_status,
                                          void*                   ui_instance)
{
    bool     has_captured = false;
    _ogl_ui* ui_ptr       = (_ogl_ui*) ui_instance;

    if (!ui_ptr->current_lbm_status)
    {
        float click_xy       [2] = {0};
        int   window_position[2] = {0};
        int   window_size    [2] = {0};

        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);
        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_POSITION,
                                   window_position);

        click_xy[0] = float(x - window_position[0]) / float(window_size[0]);
        click_xy[1] = float(y - window_position[1]) / float(window_size[1]);

        /* Iterate through controls and see if any is in the area. If so, notify of the event
         *
         * NOTE: This has got nothing to do with rendering - the handler will NOT be called from a rendering thread.
         */
        size_t n_controls = system_resizable_vector_get_amount_of_elements(ui_ptr->controls);

        for (size_t n_control = 0;
                    n_control < n_controls;
                  ++n_control)
        {
            _ogl_ui_control* control_ptr = NULL;

            if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                       n_control,
                                                      &control_ptr) )
            {
                if (control_ptr->pfn_is_over_func_ptr != NULL &&
                    control_ptr->pfn_is_over_func_ptr(control_ptr->internal,
                                                      click_xy) )
                {
                    control_ptr->pfn_on_lbm_down_func_ptr(control_ptr->internal,
                                                          click_xy);

                    has_captured = true;
                    break;
                } /* if (control_ptr->pfn_is_over_func_ptr(control_ptr->internal, click_xy) ) */
            } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &control_ptr) ) */
        } /* foreach (control) */

        ui_ptr->current_lbm_status = true;
    } /* if (!ui_ptr->current_lbm_status) */

    return !has_captured;
}

/** TODO */
PRIVATE bool _ogl_ui_callback_on_lbm_up(system_window,
                                        LONG                    x,
                                        LONG                    y,
                                        system_window_vk_status,
                                        void*                   ui_instance)
{
    _ogl_ui* ui_ptr = (_ogl_ui*) ui_instance;

    if (ui_ptr->current_lbm_status)
    {
        float click_xy       [2] = {0};
        int   window_position[2] = {0};
        int   window_size    [2] = {0};

        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);
        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_POSITION,
                                   window_position);

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            click_xy[0] = float(x - window_position[0]) / float(window_size[0]);
            click_xy[1] = float(y - window_position[1]) / float(window_size[1]);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);

        /* Iterate through all controls and ping all controls about the event. This must not
         * be limited to controls that the user is hovering over, because the user might have
         * clicked on a control and then moved outside the reach of the widget.
         *
         * NOTE: This has got nothing to do with rendering - the handler will NOT be called from a rendering thread.
         */
        size_t n_controls = system_resizable_vector_get_amount_of_elements(ui_ptr->controls);

        for (size_t n_control = 0;
                    n_control < n_controls;
                  ++n_control)
        {
            _ogl_ui_control* control_ptr = NULL;

            if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                       n_control,
                                                      &control_ptr) )
            {
                if (control_ptr->pfn_on_lbm_up_func_ptr != NULL)
                {
                    control_ptr->pfn_on_lbm_up_func_ptr(control_ptr->internal,
                                                        click_xy);
                }
            } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &control_ptr) ) */
        } /* foreach (control) */

        ui_ptr->current_lbm_status = false;
    } /* if (ui_ptr->current_lbm_status) */

    return true;
}

/** TODO */
PRIVATE bool _ogl_ui_callback_on_mouse_move(system_window           window,
                                            LONG                    x,
                                            LONG                    y,
                                            system_window_vk_status vk_status,
                                            void*                   ui_instance)
{
    int      window_position[2] = {0};
    int      window_size    [2] = {0};
    _ogl_ui* ui_ptr             = (_ogl_ui*) ui_instance;

    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);
    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_POSITION,
                               window_position);

    system_critical_section_enter(ui_ptr->rendering_cs);
    {
        ui_ptr->current_mouse_xy[0] = float(x - window_position[0]) / float(window_size[0]);
        ui_ptr->current_mouse_xy[1] = float(y - window_position[1]) / float(window_size[1]);
    }
    system_critical_section_leave(ui_ptr->rendering_cs);

    /* Iterate through controls and see if any is in the area. If so, notify of the event 
     *
     * NOTE: This has got nothing to do with rendering - the handler will NOT be called from a rendering thread.
     */
    size_t n_controls = system_resizable_vector_get_amount_of_elements(ui_ptr->controls);

    for (size_t n_control = 0;
                n_control < n_controls;
              ++n_control)
    {
        _ogl_ui_control* control_ptr = NULL;

        if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                   n_control,
                                                  &control_ptr) )
        {
            if (control_ptr->pfn_is_over_func_ptr       != NULL           &&
                control_ptr->pfn_on_mouse_move_func_ptr != NULL           &&
                control_ptr->pfn_is_over_func_ptr(control_ptr->internal,
                                                  ui_ptr->current_mouse_xy))
            {
                control_ptr->pfn_on_mouse_move_func_ptr(control_ptr->internal,
                                                        ui_ptr->current_mouse_xy);

                break;
            } /* if (control_ptr->pfn_is_over_func_ptr(control_ptr->internal, ui_ptr->current_mouse_xy) ) */
            
            if (control_ptr->pfn_on_mouse_move_func_ptr != NULL)
            {
                control_ptr->pfn_on_mouse_move_func_ptr(control_ptr->internal,
                                                        ui_ptr->current_mouse_xy);
            }
        } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &control_ptr) ) */
    } /* foreach (control) */

    return true;
}

/** TODO */
PRIVATE bool _ogl_ui_callback_on_mouse_wheel(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             short                   wheel_delta,
                                             system_window_vk_status vk_status,
                                             void*                   ui_instance)
{
    bool     handled            = false;
    int      window_position[2] = {0};
    int      window_size    [2] = {0};
    _ogl_ui* ui_ptr             = (_ogl_ui*) ui_instance;

    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);
    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_POSITION,
                               window_position);

    system_critical_section_enter(ui_ptr->rendering_cs);
    {
        ui_ptr->current_mouse_xy[0] = float(x - window_position[0]) / float(window_size[0]);
        ui_ptr->current_mouse_xy[1] = float(y - window_position[1]) / float(window_size[1]);
    }
    system_critical_section_leave(ui_ptr->rendering_cs);

    /* Iterate through controls and see if any is in the area. If so, notify of the event
     *
     * NOTE: This has got nothing to do with rendering - the handler will NOT be called from a rendering thread.
     */
    size_t n_controls = system_resizable_vector_get_amount_of_elements(ui_ptr->controls);

    for (size_t n_control = 0;
                n_control < n_controls;
              ++n_control)
    {
        _ogl_ui_control* control_ptr = NULL;

        if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                   n_control,
                                                  &control_ptr) )
        {
            if (control_ptr->pfn_is_over_func_ptr        != NULL              &&
                control_ptr->pfn_on_mouse_wheel_func_ptr != NULL              &&
                control_ptr->pfn_is_over_func_ptr(control_ptr->internal,
                                                  ui_ptr->current_mouse_xy) )
            {
                control_ptr->pfn_on_mouse_wheel_func_ptr(control_ptr->internal,
                                                         float(wheel_delta) / float(WHEEL_DELTA) );

                handled = true;
                break;
            }
        } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &control_ptr) ) */
    } /* foreach (control) */

    return !handled;
}

/** TODO */
PRIVATE void _ogl_ui_release(__in __notnull __deallocate(mem) void* data_ptr)
{
    _ogl_ui_deinit( (_ogl_ui*) data_ptr);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_button(__in           __notnull   ogl_ui                    ui_instance,
                                                    __in           __notnull   system_hashed_ansi_string name,
                                                    __in_ecount(2) __notnull   const float*              x1y1,
                                                    __in           __notnull   PFNOGLUIFIREPROCPTR       pfn_fire_ptr,
                                                    __in           __maybenull void*                     fire_user_arg)
{
    _ogl_ui_control* new_ui_control_ptr = new (std::nothrow) _ogl_ui_control;
    _ogl_ui*         ui_ptr             = (_ogl_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal   = NULL;
        int   window_size[2] = {0};
        float x2y2       [2] = {0};

        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        x2y2[0]      = x1y1[0] + 100.0f / window_size[0];
        x2y2[1]      = x1y1[1] + 20.0f  / window_size[1];
        new_internal = ogl_ui_button_init(ui_instance,
                                          ui_ptr->text_renderer,
                                          name,
                                          x1y1,
                                          x2y2,
                                          pfn_fire_ptr,
                                          fire_user_arg);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ogl_ui_control) );

        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ogl_ui_button_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ogl_ui_button_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ogl_ui_button_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ogl_ui_button_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ogl_ui_button_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ogl_ui_button_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ogl_ui_button_set_property;

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);
    }

    return (ogl_ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_checkbox(__in           __notnull   ogl_ui                    ui_instance,
                                                      __in           __notnull   system_hashed_ansi_string name,
                                                      __in_ecount(2) __notnull   const float*              x1y1,
                                                      __in                       bool                      default_status,
                                                      __in           __notnull   PFNOGLUIFIREPROCPTR       pfn_fire_ptr,
                                                      __in           __maybenull void*                     fire_user_arg)
{
    _ogl_ui_control* new_ui_control_ptr = new (std::nothrow) _ogl_ui_control;
    _ogl_ui*         ui_ptr             = (_ogl_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");

    if (new_ui_control_ptr != NULL)
    {
        void* new_internal   = NULL;
        int   window_size[2] = {0};
        float x2y2       [2] = {0};

        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        x2y2[0]      = x1y1[0] + 100.0f / window_size[0];
        x2y2[1]      = x1y1[1] + 20.0f  / window_size[1];
        new_internal = ogl_ui_checkbox_init(ui_instance,
                                            ui_ptr->text_renderer,
                                            name,
                                            x1y1,
                                            pfn_fire_ptr,
                                            fire_user_arg,
                                            default_status);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ogl_ui_control) );

        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ogl_ui_checkbox_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ogl_ui_checkbox_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ogl_ui_checkbox_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ogl_ui_checkbox_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ogl_ui_checkbox_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ogl_ui_checkbox_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ogl_ui_checkbox_set_property;

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);
    }

    return (ogl_ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_dropdown(__in                   __notnull   ogl_ui                     ui_instance,
                                                      __in                               uint32_t                   n_entries,
                                                      __in_ecount(n_entries) __notnull   system_hashed_ansi_string* strings,
                                                      __in_ecount(n_entries) __notnull   void**                     user_args,
                                                      __in                               uint32_t                   n_selected_entry,
                                                      __in                   __notnull   system_hashed_ansi_string  name,
                                                      __in_ecount(2)         __notnull   const float*               x1y1,
                                                      __in                   __notnull   PFNOGLUIFIREPROCPTR        pfn_fire_ptr,
                                                      __in                   __maybenull void*                      fire_user_arg)
{
    _ogl_ui_control* new_ui_control_ptr = new (std::nothrow) _ogl_ui_control;
    _ogl_ui*         ui_ptr             = (_ogl_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal   = NULL;
        int   window_size[2] = {0};
        float x2y2       [2] = {0};

        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        new_internal = ogl_ui_dropdown_init(ui_instance,
                                            ui_ptr->text_renderer,
                                            name,
                                            n_entries,
                                            strings,
                                            user_args,
                                            n_selected_entry,
                                            name,
                                            x1y1,
                                            pfn_fire_ptr,
                                            fire_user_arg,
                                            (ogl_ui_control) new_ui_control_ptr);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ogl_ui_control) );

        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ogl_ui_dropdown_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ogl_ui_dropdown_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ogl_ui_dropdown_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ogl_ui_dropdown_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ogl_ui_dropdown_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ogl_ui_dropdown_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = ogl_ui_dropdown_on_mouse_move;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = ogl_ui_dropdown_on_mouse_wheel;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ogl_ui_dropdown_set_property;

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);
    }

    return (ogl_ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_frame(__in           __notnull ogl_ui       ui_instance,
                                                   __in_ecount(4) __notnull const float* x1y1x2y2)
{
    _ogl_ui_control* new_ui_control_ptr = new (std::nothrow) _ogl_ui_control;
    _ogl_ui*         ui_ptr             = (_ogl_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal  = NULL;

        new_internal = ogl_ui_frame_init(ui_instance,
                                         x1y1x2y2);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ogl_ui_control) );

        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ogl_ui_frame_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ogl_ui_frame_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ogl_ui_frame_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = NULL;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = NULL;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = NULL;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ogl_ui_frame_set_property;

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);
    }

    return (ogl_ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_label(__in           __notnull ogl_ui                    ui_instance,
                                                   __in           __notnull system_hashed_ansi_string name,
                                                   __in_ecount(2) __notnull const float*              x1y1)
{
    _ogl_ui_control* new_ui_control_ptr = new (std::nothrow) _ogl_ui_control;
    _ogl_ui*         ui_ptr             = (_ogl_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal  = NULL;

        new_internal = ogl_ui_label_init(ui_instance,
                                         ui_ptr->text_renderer,
                                         name,
                                         x1y1);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ogl_ui_control) );

        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ogl_ui_label_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = NULL;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ogl_ui_label_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = NULL;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = NULL;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = NULL;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ogl_ui_label_set_property;

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);
    }

    return (ogl_ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_scrollbar(__in           __notnull   ogl_ui                         ui_instance,
                                                       __in           __notnull   system_hashed_ansi_string      name,
                                                       __in           __notnull   system_variant                 min_value,
                                                       __in           __notnull   system_variant                 max_value,
                                                       __in_ecount(2) __notnull   const float*                   x1y1,
                                                       __in           __notnull   PFNOGLUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                                       __in           __maybenull void*                          get_current_value_ptr_user_arg,
                                                       __in           __notnull   PFNOGLUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                                       __in           __maybenull void*                          set_current_value_ptr_user_arg)
{
    _ogl_ui_control* new_ui_control_ptr = new (std::nothrow) _ogl_ui_control;
    _ogl_ui*         ui_ptr             = (_ogl_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal   = NULL;
        int   window_size[2] = {0};
        float x2y2       [2] = {0};

        system_window_get_property(ui_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        x2y2[0] = x1y1[0] + 100.0f / window_size[0];
        x2y2[1] = x1y1[1] + 36.0f  / window_size[1];

        new_internal = ogl_ui_scrollbar_init(ui_instance,
                                             ui_ptr->text_renderer,
                                             name,
                                             min_value,
                                             max_value,
                                             x1y1,
                                             x2y2,
                                             pfn_get_current_value_ptr,
                                             get_current_value_ptr_user_arg,
                                             pfn_set_current_value_ptr,
                                             set_current_value_ptr_user_arg);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ogl_ui_control) );

        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ogl_ui_scrollbar_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ogl_ui_scrollbar_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ogl_ui_scrollbar_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ogl_ui_scrollbar_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ogl_ui_scrollbar_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ogl_ui_scrollbar_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = ogl_ui_scrollbar_on_mouse_move;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ogl_ui_scrollbar_set_property;

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);
    }

    return (ogl_ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui_control ogl_ui_add_texture_preview(__in __notnull           ogl_ui                      ui_instance,
                                                             __in __notnull           system_hashed_ansi_string   name,
                                                             __in_ecount(2) __notnull const float*                x1y1,
                                                             __in_ecount(2) __notnull const float*                max_size,
                                                             __in_opt                 ogl_texture                 texture,
                                                             __in                     ogl_ui_texture_preview_type preview_type)
{
    _ogl_ui_control* new_ui_control_ptr = new (std::nothrow) _ogl_ui_control;
    _ogl_ui*         ui_ptr             = (_ogl_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal  = NULL;

        new_internal = ogl_ui_texture_preview_init(ui_instance,
                                                   ui_ptr->text_renderer,
                                                   name,
                                                   x1y1,
                                                   max_size,
                                                   texture,
                                                   preview_type);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ogl_ui_control) );

        new_ui_control_ptr->internal                   = new_internal;
        new_ui_control_ptr->pfn_deinit_func_ptr        = ogl_ui_texture_preview_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr          = ogl_ui_texture_preview_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr  = ogl_ui_texture_preview_get_property;
        new_ui_control_ptr->pfn_set_property_func_ptr  = ogl_ui_texture_preview_set_property;

        system_critical_section_enter(ui_ptr->rendering_cs);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_critical_section_leave(ui_ptr->rendering_cs);
    }

    return (ogl_ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui ogl_ui_create(__in __notnull ogl_text                  text_renderer,
                                        __in __notnull system_hashed_ansi_string name)
{
    _ogl_ui* ui_ptr = new (std::nothrow) _ogl_ui;

    ASSERT_ALWAYS_SYNC(ui_ptr != NULL, "Out of memory");
    if (ui_ptr != NULL)
    {
        _ogl_ui_init(ui_ptr,
                     name,
                     text_renderer);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(ui_ptr,
                                                       _ogl_ui_release,
                                                       OBJECT_TYPE_OGL_UI,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL UIs\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ogl_ui) ui_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_ui_draw(__in __notnull ogl_ui ui)
{
    _ogl_ui*                          ui_ptr      = (_ogl_ui*) ui;
    ogl_context                       context     = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints* entrypoints = NULL;
    size_t                            n_controls  = system_resizable_vector_get_amount_of_elements(ui_ptr->controls);
    GLuint                            vao_id      = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    ui_ptr->pGLDisable        (GL_CULL_FACE);
    ui_ptr->pGLBindVertexArray(vao_id);

    for (size_t n_control = 0;
                n_control < n_controls;
              ++n_control)
    {
        _ogl_ui_control* ui_control_ptr = NULL;

        if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                   n_control,
                                                  &ui_control_ptr) )
        {
            if (ui_control_ptr->pfn_draw_func_ptr != NULL)
            {
                ui_control_ptr->pfn_draw_func_ptr(ui_control_ptr->internal);
            }
        } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &ui_control_ptr) */
    } /* for (size_t n_control = 0; n_control < n_controls; ++n_control) */
}

/** Please see header for speciication */
PUBLIC EMERALD_API void ogl_ui_get_property(__in  __notnull ogl_ui_control control,
                                            __in            int            property_value,
                                            __out __notnull void*          out_result)
{
    _ogl_ui_control* ui_control_ptr = (_ogl_ui_control*) control;

    ASSERT_DEBUG_SYNC(ui_control_ptr->pfn_get_property_func_ptr != NULL,
                      "Requested user control does not support property getters");

    if (ui_control_ptr->pfn_get_property_func_ptr != NULL)
    {
        ui_control_ptr->pfn_get_property_func_ptr(ui_control_ptr->internal,
                                                  property_value,
                                                  out_result);
    }
}

/** Please see header for specification */
PUBLIC ogl_context ogl_ui_get_context(__in __notnull ogl_ui ui)
{
    return ogl_text_get_context( ((_ogl_ui*) ui)->text_renderer);
}

/** Please see header for specification */
PUBLIC ogl_program ogl_ui_get_registered_program(__in __notnull ogl_ui                    ui,
                                                 __in __notnull system_hashed_ansi_string name)
{
    ogl_program result = NULL;
    _ogl_ui*    ui_ptr = (_ogl_ui*) ui;

    system_hash64map_get(ui_ptr->registered_ui_control_programs,
                         system_hashed_ansi_string_get_hash(name),
                        &result);

    if (result != NULL)
    {
        ogl_program_retain(result);
    }

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_ui_receive_control_callback(__in     __notnull ogl_ui         ui,
                                            __in     __notnull ogl_ui_control control,
                                            __in     __notnull int            callback_id,
                                            __in_opt           void*          callback_user_arg)
{
    system_resizable_vector callback_vector = NULL;
    _ogl_ui*                ui_ptr          = (_ogl_ui*) ui;

    /* Check if there is any callback vector associated with the control */
    if (system_hash64map_get(ui_ptr->registered_ui_control_callbacks,
                             (system_hash64) control,
                            &callback_vector))
    {
        const unsigned int n_callbacks = system_resizable_vector_get_amount_of_elements(callback_vector);

        for (unsigned int n_callback = 0;
                          n_callback < n_callbacks;
                        ++n_callback)
        {
            _ogl_ui_callback* callback_ptr = NULL;

            if (system_resizable_vector_get_element_at(callback_vector,
                                                       n_callback,
                                                      &callback_ptr) )
            {
                if (callback_ptr->callback_id == callback_id)
                {
                    callback_ptr->callback_proc_ptr(control,
                                                    callback_id,
                                                    callback_ptr->callback_proc_user_arg,
                                                    callback_user_arg);
                }
            }
        } /* for (all callbacks) */
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_ui_register_control_callback(__in __notnull ogl_ui                       ui,
                                                         __in __notnull ogl_ui_control               control,
                                                         __in __notnull int                          callback_id,
                                                         __in __notnull PFNOGLUIEVENTCALLBACKPROCPTR callback_proc_ptr,
                                                         __in __notnull void*                        callback_proc_user_arg)
{
    system_resizable_vector callback_vector      = NULL;
    void*                   internal_control_ptr = _ogl_ui_get_internal_control_ptr(control);
    _ogl_ui*                ui_ptr               = (_ogl_ui*) ui;

    /* If there's no callback vector associated with the control, spawn one now */
    if (!system_hash64map_get(ui_ptr->registered_ui_control_callbacks,
                              (system_hash64) internal_control_ptr,
                             &callback_vector) )
    {
        callback_vector = system_resizable_vector_create(4, /* capacity */
                                                         sizeof(_ogl_ui_callback*) );

        ASSERT_ALWAYS_SYNC(callback_vector != NULL,
                           "Could not create a callback vector");

        system_hash64map_insert(ui_ptr->registered_ui_control_callbacks,
                                (system_hash64) internal_control_ptr,
                                callback_vector,
                                NULL,  /* on_remove_callback_proc */
                                NULL); /* on_remove_callback_proc_user_arg */
    }

    /* Spawn the callback descriptor */
    _ogl_ui_callback* new_callback = new (std::nothrow) _ogl_ui_callback;

    ASSERT_ALWAYS_SYNC(new_callback != NULL,
                       "Out of memory");
    if (new_callback != NULL)
    {
        new_callback->callback_id            = callback_id;
        new_callback->callback_proc_ptr      = callback_proc_ptr;
        new_callback->callback_proc_user_arg = callback_proc_user_arg;

        /* Store it */
        system_resizable_vector_push(callback_vector,
                                     new_callback);
    }
}

/** Please see header for specification */
PUBLIC bool ogl_ui_register_program(__in __notnull ogl_ui                    ui,
                                    __in __notnull system_hashed_ansi_string program_name,
                                    __in __notnull ogl_program               program)
{
    const system_hash64 program_name_hash = system_hashed_ansi_string_get_hash(program_name);
    bool                result            = false;
    _ogl_ui*            ui_ptr            = (_ogl_ui*) ui;

    if (!system_hash64map_contains(ui_ptr->registered_ui_control_programs,
                                   program_name_hash) )
    {
        system_hash64map_insert(ui_ptr->registered_ui_control_programs,
                                program_name_hash,
                                program,
                                NULL,
                                NULL);

        ogl_program_retain(program);
        result = true;
    } /* if (!system_hash64map_contains(ui_ptr->registered_ui_control_programs, program_name_hash) ) */
    else
    {
        LOG_ERROR("UI program [%s] will not be registered - already stored",
                  system_hashed_ansi_string_get_buffer(ogl_program_get_name(program) ) );
    }

    return result;
}

/** Please see header for speciication */
PUBLIC EMERALD_API void ogl_ui_set_property(__in __notnull ogl_ui_control control,
                                            __in           int            property_value,
                                            __in __notnull const void*    data)
{
    _ogl_ui_control* ui_control_ptr = (_ogl_ui_control*) control;

    ASSERT_DEBUG_SYNC(ui_control_ptr->pfn_set_property_func_ptr != NULL,
                      "Requested user control does not support property setters");

    if (ui_control_ptr->pfn_set_property_func_ptr != NULL)
    {
        ui_control_ptr->pfn_set_property_func_ptr(ui_control_ptr->internal,
                                                  property_value,
                                                  data);
    }
}