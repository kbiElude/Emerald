/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * NOTE: UI renderer is built under assumption that UI components need not use attributes.
 *       This is owing to the fact ui uses the context-wide no-VAA VAO for rendering.
 *       Change this if necessary (but face the performance penalty due to VAO rebindings).
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_text.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_button.h"
#include "ui/ui_checkbox.h"
#include "ui/ui_dropdown.h"
#include "ui/ui_frame.h"
#include "ui/ui_label.h"
#include "ui/ui_label.h"
#include "ui/ui_scrollbar.h"
#include "ui/ui_texture_preview.h"

/** Internal definitions */
#define N_START_CONTROLS     (4)

/** Internal types */
typedef void (*PFNUIDEINITPROCPTR)        (void*               internal_instance);
typedef void (*PFNUIDRAWPROCPTR)          (void*               internal_instance);
typedef void (*PFNUIGETPROPERTYPROCPTR)   (const void*         internal_instance,
                                           ui_control_property property,
                                           void*               out_result);
typedef void (*PFNUIHOVERPROCPTR)         (void*               internal_instance,
                                           const float*        xy_screen_norm);
typedef bool (*PFNUIISOVERPROCPTR)        (void*               internal_instance,
                                           const float*        xy);
typedef void (*PFNUIONLBMDOWNPROCPTR)     (void*               internal_instance,
                                           const float*        xy);
typedef void (*PFNUIONLBMUPPROCPTR)       (void*               internal_instance,
                                           const float*        xy);
typedef void (*PFNUIONMOUSEMOVEPROCPTR)   (void*               internal_instance,
                                           const float*        xy);
typedef void (*PFNUIONMOUSEWHEELPROCPTR)  (void*               internal_instance,
                                           float               wheel_delta);
typedef void (*PFNUISETPROPERTYPROCPTR)   (void*               internal_instance,
                                           ui_control_property property,
                                              const void*         data);

typedef struct _ui_callback
{
    int                       callback_id;
    PFNUIEVENTCALLBACKPROCPTR callback_proc_ptr;
    void*                     callback_proc_user_arg;

    _ui_callback()
    {
        callback_id            = 0;
        callback_proc_ptr      = NULL;
        callback_proc_user_arg = NULL;
    }
} _ui_callback;

typedef struct
{
    float xy[2];
} _ui_click;

typedef struct
{
    system_resizable_vector   controls;
    system_read_write_mutex   controls_rw_mutex;
    system_hashed_ansi_string name;
    ogl_text                  text_renderer;
    system_window             window;

    system_hash64map registered_ui_control_callbacks; /* stores a system_resizable_vector storing _ui_callback instances.
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
} _ui;

typedef struct _ui_control
{
    _ui_control_type control_type;
    _ui*             owner_ptr;

    /* NOT called from a rendering thread */
    PFNUIDEINITPROCPTR pfn_deinit_func_ptr;

    /* CALLED from a rendering thread */
    PFNUIDRAWPROCPTR pfn_draw_func_ptr;

    /* NOT called from a rendering thread */
    PFNUIGETPROPERTYPROCPTR pfn_get_property_func_ptr;

    /* NOT called from a rendering thread */
    PFNUIISOVERPROCPTR pfn_is_over_func_ptr;

    /* NOT called from a rendering thread */
    PFNUIONLBMDOWNPROCPTR pfn_on_lbm_down_func_ptr;

    /* NOT called from a rendering thread */
    PFNUIONLBMUPPROCPTR pfn_on_lbm_up_func_ptr;

    /* NOT called from a rendering thread */
    PFNUIONMOUSEMOVEPROCPTR pfn_on_mouse_move_func_ptr;

    /* NOT called from a rendering thread */
    PFNUIONMOUSEWHEELPROCPTR pfn_on_mouse_wheel_func_ptr;

    /* NOT called from a renderingt thread */
    PFNUISETPROPERTYPROCPTR pfn_set_property_func_ptr;

    void* internal;

    _ui_control()
    {
        control_type                = UI_CONTROL_TYPE_UNKNOWN;
        owner_ptr                   = NULL;
        pfn_deinit_func_ptr         = NULL;
        pfn_draw_func_ptr           = NULL;
        pfn_get_property_func_ptr   = NULL;
        pfn_is_over_func_ptr        = NULL;
        pfn_on_lbm_down_func_ptr    = NULL;
        pfn_on_lbm_up_func_ptr      = NULL;
        pfn_on_mouse_move_func_ptr  = NULL;
        pfn_on_mouse_wheel_func_ptr = NULL;
        pfn_set_property_func_ptr   = NULL;
    }
} _ui_control;

/* Forward declarations */
PRIVATE void _ui_control_deinit           (_ui_control*              control_ptr);
PRIVATE void _ui_control_init             (_ui_control*              control_ptr);
PRIVATE void _ui_deinit                   (_ui*                      ui_ptr);
PRIVATE void _ui_init                     (_ui*                      ui_ptr,
                                           system_hashed_ansi_string name,
                                           ogl_text                  text_renderer);
PRIVATE bool _ui_callback_on_lbm_down     (system_window             window,
                                           int                       x,
                                           int                       y,
                                           system_window_vk_status   key_status,
                                           void*                     user_arg);
PRIVATE bool _ui_callback_on_lbm_up       (system_window             window,
                                           int                       x,
                                           int                       y,
                                           system_window_vk_status   key_status,
                                           void*                     user_arg);
PRIVATE bool _ui_callback_on_mouse_move   (system_window             window,
                                           int                       x,
                                           int                       y,
                                           system_window_vk_status   key_status,
                                           void*                     user_arg);
PRIVATE bool _ui_callback_on_mouse_wheel  (system_window             window,
                                           int                       x,
                                           int                       y,
                                           short                     scroll_delta,
                                           system_window_vk_status   key_status,
                                           void*                     user_arg);
PRIVATE void* _ui_get_internal_control_ptr(ui_control                control);
PRIVATE void  _ui_release                 (void*                     ui);


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ui,
                               ui,
                              _ui);

/** TODO */
PRIVATE void _ui_control_deinit(_ui_control* ui_control_ptr)
{
    if (ui_control_ptr->pfn_deinit_func_ptr != NULL)
    {
        ui_control_ptr->pfn_deinit_func_ptr(ui_control_ptr->internal);

        ui_control_ptr->internal = NULL;
    } /* if (ui_control_ptr->pfn_deinit_func_ptr != NULL) */
}

/** TODO */
PRIVATE void* _ui_get_internal_control_ptr(ui_control control)
{
    _ui_control* control_ptr = (_ui_control*) control;

    return control_ptr->internal;
}

/** TODO */
PRIVATE void _ui_control_init(_ui_control* ui_control_ptr)
{
    memset(ui_control_ptr,
           0,
           sizeof(_ui_control) );
}

/** TODO */
PRIVATE void _ui_deinit_gl_renderer_callback(ogl_context context,
                                             void*       user_arg)
{
    ral_backend_type            backend_type          = RAL_BACKEND_TYPE_UNKNOWN;
    PFNGLDELETEVERTEXARRAYSPROC pGLDeleteVertexArrays = NULL;
    _ui*                        ui_ptr                = (_ui*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    if (backend_type == RAL_BACKEND_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        pGLDeleteVertexArrays = entry_points->pGLDeleteVertexArrays;
    }
    else
    {
        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "Unsupportedbackend type");

        const ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        pGLDeleteVertexArrays = entry_points->pGLDeleteVertexArrays;
    }
}

/** TODO */
PRIVATE void _ui_deinit(_ui* ui_ptr)
{
    _ui_control* ui_control_ptr = NULL;

    /* Release call-backs */
    system_window_delete_callback_func(ui_ptr->window,
                                       SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                       (void*) _ui_callback_on_lbm_down,
                                       ui_ptr);
    system_window_delete_callback_func(ui_ptr->window,
                                       SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                       (void*) _ui_callback_on_lbm_up,
                                       ui_ptr);
    system_window_delete_callback_func(ui_ptr->window,
                                       SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                       (void*) _ui_callback_on_mouse_move,
                                       ui_ptr);

    /* Release GL stuff. */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(ogl_text_get_context(ui_ptr->text_renderer)),
                                                     _ui_deinit_gl_renderer_callback,
                                                     ui_ptr);

    /* Release all controls */
    if (ui_ptr->controls_rw_mutex != NULL)
    {
        system_read_write_mutex_release(ui_ptr->controls_rw_mutex);

        ui_ptr->controls_rw_mutex = NULL;
    }

    while (true)
    {
        if (!system_resizable_vector_get_element_at(ui_ptr->controls,
                                                    0,
                                                   &ui_control_ptr) )
        {
            break;
        }
        else
        {
            _ui_control_deinit(ui_control_ptr);

            delete ui_control_ptr;
            ui_control_ptr = NULL;

            system_resizable_vector_delete_element_at(ui_ptr->controls,
                                                      0);
        }
    }

    system_resizable_vector_release(ui_ptr->controls);
    ui_ptr->controls = NULL;

    /* Release other owned objects */
    if (ui_ptr->text_renderer != NULL)
    {
        ogl_text_release(ui_ptr->text_renderer);

        ui_ptr->text_renderer = NULL;
    }

    if (ui_ptr->registered_ui_control_callbacks != NULL)
    {
        while (true)
        {
            unsigned int n_callbacks = 0;

            system_hash64map_get_property(ui_ptr->registered_ui_control_callbacks,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_callbacks);

            if (n_callbacks == 0)
            {
                break;
            }

            bool          result           = false;
            system_hash64 ui_callback_hash = 0;
            _ui_callback* ui_callback_ptr  = NULL;

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
        while (true)
        {
            uint32_t      n_programs                   = 0;
            bool          result                       = false;
            ral_program   ui_control_program           = NULL;
            system_hash64 ui_control_program_name_hash = 0;

            system_hash64map_get_property(ui_ptr->registered_ui_control_programs,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_programs);

            if (n_programs == 0)
            {
                break;
            }

            if (system_hash64map_get_element_at(ui_ptr->registered_ui_control_programs,
                                                0,
                                               &ui_control_program,
                                               &ui_control_program_name_hash) )
            {
                ral_context context = NULL;

                system_window_get_property(ui_ptr->window,
                                           SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT_RAL,
                                          &context);

                ral_context_delete_objects(context,
                                           RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                           1, /* n_objects */
                                           (const void**) &ui_control_program);

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
PRIVATE void _ui_init_gl_renderer_callback(ogl_context context,
                                           void*       user_arg)
{
    ral_backend_type         backend_type       = RAL_BACKEND_TYPE_UNKNOWN;
    PFNGLGENVERTEXARRAYSPROC pGLGenVertexArrays = NULL;
    _ui*                     ui_ptr             = (_ui*) user_arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    if (backend_type == RAL_BACKEND_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        pGLGenVertexArrays = entry_points->pGLGenVertexArrays;
    }
    else
    {
        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "Unsupported context type");

        const ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        pGLGenVertexArrays = entry_points->pGLGenVertexArrays;
    }
}

/** TODO */
PRIVATE void _ui_init(_ui*                      ui_ptr,
                      system_hashed_ansi_string name,
                      ogl_text                  text_renderer)
{
    ral_context context = ogl_text_get_context(text_renderer);

    ui_ptr->controls                        = system_resizable_vector_create(N_START_CONTROLS);
    ui_ptr->controls_rw_mutex               = system_read_write_mutex_create();
    ui_ptr->current_lbm_status              = false;
    ui_ptr->current_mouse_xy[0]             = 0;
    ui_ptr->current_mouse_xy[1]             = 0;
    ui_ptr->name                            = name;
    ui_ptr->registered_ui_control_callbacks = system_hash64map_create(sizeof(void*) );
    ui_ptr->registered_ui_control_programs  = system_hash64map_create(sizeof(void*) );
    ui_ptr->text_renderer                   = text_renderer;
    ui_ptr->window                          = NULL;

    ogl_text_retain(text_renderer);

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                            &ui_ptr->window);

    /* Cache GL func ptrs that will be used by the draw routine */
    ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    if (backend_type == RAL_BACKEND_TYPE_ES)
    {
        ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(ral_context_get_gl_context(context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        ui_ptr->pGLBindVertexArray = entry_points->pGLBindVertexArray;
        ui_ptr->pGLDisable         = entry_points->pGLDisable;
    }
    else
    {
        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "Unrecognized context type");

        ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(ral_context_get_gl_context(context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        ui_ptr->pGLBindVertexArray = entry_points->pGLBindVertexArray;
        ui_ptr->pGLDisable         = entry_points->pGLDisable;
    }

    /* Sign up for mouse events */
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                    (void*) _ui_callback_on_lbm_down,
                                    ui_ptr);
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                    (void*) _ui_callback_on_lbm_up,
                                    ui_ptr);
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                    (void*) _ui_callback_on_mouse_move,
                                    ui_ptr);
    system_window_add_callback_func(ui_ptr->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL,
                                    (void*) _ui_callback_on_mouse_wheel,
                                    ui_ptr);

    /* Create GL-specific objects */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                     _ui_init_gl_renderer_callback,
                                                     ui_ptr);
}

/** TODO */
PRIVATE bool _ui_callback_on_lbm_down(system_window           window,
                                      int                     x,
                                      int                     y,
                                      system_window_vk_status vk_status,
                                      void*                   ui_instance)
{
    bool has_captured = false;
    _ui* ui_ptr       = (_ui*) ui_instance;

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
        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_READ);
        {
            uint32_t n_controls = 0;

            system_resizable_vector_get_property(ui_ptr->controls,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_controls);

            for (size_t n_control = 0;
                        n_control < n_controls;
                      ++n_control)
            {
                _ui_control* control_ptr = NULL;

                if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                           n_control,
                                                          &control_ptr) )
                {
                    bool is_visible = true;

                    control_ptr->pfn_get_property_func_ptr(control_ptr->internal,
                                                           UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                                          &is_visible);

                    if (!is_visible)
                    {
                        continue;
                    }

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
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_READ);

        ui_ptr->current_lbm_status = true;
    } /* if (!ui_ptr->current_lbm_status) */

    return !has_captured;
}

/** TODO */
PRIVATE bool _ui_callback_on_lbm_up(system_window           window,
                                    int                     x,
                                    int                     y,
                                    system_window_vk_status key_status,
                                    void*                   ui_instance)
{
    _ui* ui_ptr = (_ui*) ui_instance;

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

        click_xy[0] = float(x - window_position[0]) / float(window_size[0]);
        click_xy[1] = float(y - window_position[1]) / float(window_size[1]);

        /* Iterate through all controls and ping all controls about the event. This must not
         * be limited to controls that the user is hovering over, because the user might have
         * clicked on a control and then moved outside the reach of the widget.
         *
         * NOTE: This has got nothing to do with rendering - the handler will NOT be called from a rendering thread.
         */
        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_READ);
        {
            uint32_t n_controls = 0;

            system_resizable_vector_get_property(ui_ptr->controls,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_controls);

            for (size_t n_control = 0;
                        n_control < n_controls;
                      ++n_control)
            {
                _ui_control* control_ptr = NULL;

                if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                           n_control,
                                                          &control_ptr) )
                {
                    bool is_visible = true;

                    control_ptr->pfn_get_property_func_ptr(control_ptr->internal,
                                                           UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                                          &is_visible);

                    if (!is_visible)
                    {
                        continue;
                    }

                    if (control_ptr->pfn_on_lbm_up_func_ptr != NULL)
                    {
                        control_ptr->pfn_on_lbm_up_func_ptr(control_ptr->internal,
                                                            click_xy);
                    }
                } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &control_ptr) ) */
            } /* foreach (control) */
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_READ);

        ui_ptr->current_lbm_status = false;
    } /* if (ui_ptr->current_lbm_status) */

    return true;
}

/** TODO */
PRIVATE bool _ui_callback_on_mouse_move(system_window           window,
                                        int                     x,
                                        int                     y,
                                        system_window_vk_status vk_status,
                                        void*                   ui_instance)
{
    int  window_position[2] = {0};
    int  window_size    [2] = {0};
    _ui* ui_ptr             = (_ui*) ui_instance;

    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);
    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_POSITION,
                               window_position);

    ui_ptr->current_mouse_xy[0] = float(x - window_position[0]) / float(window_size[0]);
    ui_ptr->current_mouse_xy[1] = float(y - window_position[1]) / float(window_size[1]);

    /* Iterate through controls and see if any is in the area. If so, notify of the event 
     *
     * NOTE: This has got nothing to do with rendering - the handler will NOT be called from a rendering thread.
     */
    system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                 ACCESS_READ);
    {
        uint32_t n_controls = 0;

        system_resizable_vector_get_property(ui_ptr->controls,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_controls);

        for (size_t n_control = 0;
                    n_control < n_controls;
                  ++n_control)
        {
            _ui_control* control_ptr = NULL;

            if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                       n_control,
                                                      &control_ptr) )
            {
                bool is_visible = true;

                control_ptr->pfn_get_property_func_ptr(control_ptr->internal,
                                                       UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                                      &is_visible);

                if (!is_visible)
                {
                    continue;
                }

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
    }
    system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                   ACCESS_READ);

    return true;
}

/** TODO */
PRIVATE bool _ui_callback_on_mouse_wheel(system_window           window,
                                         int                     x,
                                         int                     y,
                                         short                   wheel_delta,
                                         system_window_vk_status vk_status,
                                         void*                   ui_instance)
{
    bool handled            = false;
    int  window_position[2] = {0};
    int  window_size    [2] = {0};
    _ui* ui_ptr             = (_ui*) ui_instance;

    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);
    system_window_get_property(ui_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_POSITION,
                               window_position);

    ui_ptr->current_mouse_xy[0] = float(x - window_position[0]) / float(window_size[0]);
    ui_ptr->current_mouse_xy[1] = float(y - window_position[1]) / float(window_size[1]);

    /* Iterate through controls and see if any is in the area. If so, notify of the event
     *
     * NOTE: This has got nothing to do with rendering - the handler will NOT be called from a rendering thread.
     */
    system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                 ACCESS_READ);
    {
        uint32_t n_controls = 0;

        system_resizable_vector_get_property(ui_ptr->controls,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_controls);

        for (size_t n_control = 0;
                    n_control < n_controls;
                  ++n_control)
        {
            _ui_control* control_ptr = NULL;

            if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                       n_control,
                                                      &control_ptr) )
            {
                bool is_visible = true;

                control_ptr->pfn_get_property_func_ptr(control_ptr->internal,
                                                       UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                                      &is_visible);

                if (!is_visible)
                {
                    continue;
                }

                if (control_ptr->pfn_is_over_func_ptr        != NULL              &&
                    control_ptr->pfn_on_mouse_wheel_func_ptr != NULL              &&
                    control_ptr->pfn_is_over_func_ptr(control_ptr->internal,
                                                      ui_ptr->current_mouse_xy) )
                {
#ifdef _WIN32
                    control_ptr->pfn_on_mouse_wheel_func_ptr(control_ptr->internal,
                                                             float(wheel_delta) / float(WHEEL_DELTA) );
#else
                    ASSERT_DEBUG_SYNC(false,
                                      "TODO");
#endif
                    handled = true;
                    break;
                }
            } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &control_ptr) ) */
        } /* foreach (control) */
    }
    system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                   ACCESS_READ);

    return !handled;
}

/** TODO */
PRIVATE void _ui_release(void* data_ptr)
{
    _ui_deinit( (_ui*) data_ptr);
}

/** Please see header for specification */
PUBLIC EMERALD_API ui_control ui_add_button(ui                        ui_instance,
                                            system_hashed_ansi_string name,
                                            const float*              x1y1,
                                            PFNUIFIREPROCPTR          pfn_fire_ptr,
                                            void*                     fire_user_arg)
{
    _ui_control* new_ui_control_ptr = new (std::nothrow) _ui_control;
    _ui*         ui_ptr             = (_ui*) ui_instance;

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
        new_internal = ui_button_init(ui_instance,
                                      ui_ptr->text_renderer,
                                      name,
                                      x1y1,
                                      x2y2,
                                      pfn_fire_ptr,
                                      fire_user_arg);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ui_control) );

        new_ui_control_ptr->control_type                = UI_CONTROL_TYPE_BUTTON;
        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->owner_ptr                   = (_ui*) ui_instance;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ui_button_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ui_button_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ui_button_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ui_button_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ui_button_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ui_button_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ui_button_set_property;

        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_WRITE);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_WRITE);
    }

    return (ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui_control ui_add_checkbox(ui                        ui_instance,
                                              system_hashed_ansi_string name,
                                              const float*              x1y1,
                                              bool                      default_status,
                                              PFNUIFIREPROCPTR          pfn_fire_ptr,
                                              void*                     fire_user_arg)
{
    _ui_control* new_ui_control_ptr = new (std::nothrow) _ui_control;
    _ui*         ui_ptr             = (_ui*) ui_instance;

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
        new_internal = ui_checkbox_init(ui_instance,
                                        ui_ptr->text_renderer,
                                        name,
                                        x1y1,
                                        pfn_fire_ptr,
                                        fire_user_arg,
                                        default_status);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ui_control) );

        new_ui_control_ptr->control_type                = UI_CONTROL_TYPE_CHECKBOX;
        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->owner_ptr                   = (_ui*) ui_instance;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ui_checkbox_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ui_checkbox_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ui_checkbox_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ui_checkbox_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ui_checkbox_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ui_checkbox_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ui_checkbox_set_property;

        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_WRITE);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_WRITE);
    }

    return (ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui_control ui_add_dropdown(ui                         ui_instance,
                                              uint32_t                   n_entries,
                                              system_hashed_ansi_string* strings,
                                              void**                     user_args,
                                              uint32_t                   n_selected_entry,
                                              system_hashed_ansi_string  name,
                                              const float*               x1y1,
                                              PFNUIFIREPROCPTR           pfn_fire_ptr,
                                              void*                      fire_user_arg)
{
    _ui_control* new_ui_control_ptr = new (std::nothrow) _ui_control;
    _ui*         ui_ptr             = (_ui*) ui_instance;

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

        new_internal = ui_dropdown_init(ui_instance,
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
                                        (ui_control) new_ui_control_ptr);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ui_control) );

        new_ui_control_ptr->control_type                = UI_CONTROL_TYPE_DROPDOWN;
        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->owner_ptr                   = (_ui*) ui_instance;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ui_dropdown_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ui_dropdown_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ui_dropdown_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ui_dropdown_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ui_dropdown_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ui_dropdown_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = ui_dropdown_on_mouse_move;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = ui_dropdown_on_mouse_wheel;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ui_dropdown_set_property;

        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_WRITE);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_WRITE);
    }

    return (ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui_control ui_add_frame(ui           ui_instance,
                                           const float* x1y1x2y2)
{
    _ui_control* new_ui_control_ptr = new (std::nothrow) _ui_control;
    _ui*         ui_ptr             = (_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal  = NULL;

        new_internal = ui_frame_init(ui_instance,
                                     x1y1x2y2);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ui_control) );

        new_ui_control_ptr->control_type                = UI_CONTROL_TYPE_FRAME;
        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->owner_ptr                   = (_ui*) ui_instance;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ui_frame_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ui_frame_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ui_frame_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = NULL;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = NULL;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = NULL;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ui_frame_set_property;

        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_WRITE);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_WRITE);
    }

    return (ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui_control ui_add_label(ui                        ui_instance,
                                           system_hashed_ansi_string name,
                                           const float*              x1y1)
{
    _ui_control* new_ui_control_ptr = new (std::nothrow) _ui_control;
    _ui*         ui_ptr             = (_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal  = NULL;

        new_internal = ui_label_init(ui_instance,
                                     ui_ptr->text_renderer,
                                     name,
                                     x1y1);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ui_control) );

        new_ui_control_ptr->control_type                = UI_CONTROL_TYPE_LABEL;
        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->owner_ptr                   = (_ui*) ui_instance;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ui_label_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = NULL;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ui_label_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = NULL;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = NULL;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = NULL;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = NULL;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ui_label_set_property;

        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_WRITE);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_WRITE);
    }

    return (ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui_control ui_add_scrollbar(ui                          ui_instance,
                                               system_hashed_ansi_string   name,
                                               ui_scrollbar_text_location  text_location,
                                               system_variant              min_value,
                                               system_variant              max_value,
                                               const float*                x1y1,
                                               PFNUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                               void*                       get_current_value_ptr_user_arg,
                                               PFNUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                               void*                       set_current_value_ptr_user_arg)
{
    _ui_control* new_ui_control_ptr = new (std::nothrow) _ui_control;
    _ui*         ui_ptr             = (_ui*) ui_instance;

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
        x2y2[1] = x1y1[1] + 36.0f / window_size[1];

        new_internal = ui_scrollbar_init(ui_instance,
                                         ui_ptr->text_renderer,
                                         text_location,
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
               sizeof(_ui_control) );

        new_ui_control_ptr->control_type                = UI_CONTROL_TYPE_SCROLLBAR;
        new_ui_control_ptr->internal                    = new_internal;
        new_ui_control_ptr->owner_ptr                   = (_ui*) ui_instance;
        new_ui_control_ptr->pfn_deinit_func_ptr         = ui_scrollbar_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr           = ui_scrollbar_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr   = ui_scrollbar_get_property;
        new_ui_control_ptr->pfn_is_over_func_ptr        = ui_scrollbar_is_over;
        new_ui_control_ptr->pfn_on_lbm_down_func_ptr    = ui_scrollbar_on_lbm_down;
        new_ui_control_ptr->pfn_on_lbm_up_func_ptr      = ui_scrollbar_on_lbm_up;
        new_ui_control_ptr->pfn_on_mouse_move_func_ptr  = ui_scrollbar_on_mouse_move;
        new_ui_control_ptr->pfn_on_mouse_wheel_func_ptr = NULL;
        new_ui_control_ptr->pfn_set_property_func_ptr   = ui_scrollbar_set_property;

        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_WRITE);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_WRITE);
    }

    return (ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui_control ui_add_texture_preview(ui                        ui_instance,
                                                     system_hashed_ansi_string name,
                                                     const float*              x1y1,
                                                     const float*              max_size,
                                                     ral_texture               texture,
                                                     ui_texture_preview_type   preview_type)
{
    _ui_control* new_ui_control_ptr = new (std::nothrow) _ui_control;
    _ui*         ui_ptr             = (_ui*) ui_instance;

    ASSERT_ALWAYS_SYNC(new_ui_control_ptr != NULL,
                       "Out of memory");
    if (new_ui_control_ptr != NULL)
    {
        void* new_internal  = NULL;

        new_internal = ui_texture_preview_init(ui_instance,
                                               ui_ptr->text_renderer,
                                               name,
                                               x1y1,
                                               max_size,
                                               texture,
                                               preview_type);

        memset(new_ui_control_ptr,
               0,
               sizeof(_ui_control) );

        new_ui_control_ptr->control_type              = UI_CONTROL_TYPE_TEXTURE_PREVIEW;
        new_ui_control_ptr->internal                  = new_internal;
        new_ui_control_ptr->owner_ptr                 = (_ui*) ui_instance;
        new_ui_control_ptr->pfn_deinit_func_ptr       = ui_texture_preview_deinit;
        new_ui_control_ptr->pfn_draw_func_ptr         = ui_texture_preview_draw;
        new_ui_control_ptr->pfn_get_property_func_ptr = ui_texture_preview_get_property;
        new_ui_control_ptr->pfn_set_property_func_ptr = ui_texture_preview_set_property;

        system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                     ACCESS_WRITE);
        {
            system_resizable_vector_push(ui_ptr->controls,
                                         new_ui_control_ptr);
        }
        system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                       ACCESS_WRITE);
    }

    return (ui_control) new_ui_control_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui ui_create(ogl_text                  text_renderer,
                                system_hashed_ansi_string name)
{
    _ui* ui_ptr = new (std::nothrow) _ui;

    ASSERT_ALWAYS_SYNC(ui_ptr != NULL,
                       "Out of memory");

    if (ui_ptr != NULL)
    {
        _ui_init(ui_ptr,
                 name,
                 text_renderer);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(ui_ptr,
                                                       _ui_release,
                                                       OBJECT_TYPE_UI,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\UIs\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ui) ui_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ui_draw(ui ui_instance)
{
    _ui*                              ui_ptr      = (_ui*) ui_instance;
    ogl_context                       context     = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints* entrypoints = NULL;
    uint32_t                          n_controls  = 0;
    GLuint                            vao_id      = 0;

    system_resizable_vector_get_property(ui_ptr->controls,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_controls);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    ui_ptr->pGLDisable        (GL_CULL_FACE);
    ui_ptr->pGLBindVertexArray(vao_id);

    for (size_t n_control = 0;
                n_control < n_controls;
              ++n_control)
    {
        _ui_control* ui_control_ptr = NULL;

        if (system_resizable_vector_get_element_at(ui_ptr->controls,
                                                   n_control,
                                                  &ui_control_ptr) )
        {
            bool is_visible = true;

            ui_control_ptr->pfn_get_property_func_ptr(ui_control_ptr->internal,
                                                      UI_CONTROL_PROPERTY_GENERAL_VISIBLE,
                                                     &is_visible);

            if (!is_visible)
            {
                continue;
            }

            if (ui_control_ptr->pfn_draw_func_ptr != NULL)
            {
                ui_control_ptr->pfn_draw_func_ptr(ui_control_ptr->internal);
            }
        } /* if (system_resizable_vector_get_element_at(ui_ptr->controls, n_control, &ui_control_ptr) */
    } /* for (size_t n_control = 0; n_control < n_controls; ++n_control) */
}

/** Please see header for speciication */
PUBLIC EMERALD_API void ui_get_control_property(ui_control          control,
                                                ui_control_property property,
                                                void*               out_result)
{
    _ui_control* ui_control_ptr = (_ui_control*) control;

    ASSERT_DEBUG_SYNC(ui_control_ptr->pfn_get_property_func_ptr != NULL,
                      "Requested user control does not support property getters");

    if (property == UI_CONTROL_PROPERTY_GENERAL_INDEX)
    {
        LOG_ERROR("Performance warning: UI_CONTROL_PROPERTY_GENERAL_INDEX query is not optimized.");

        uint32_t result = system_resizable_vector_find(ui_control_ptr->owner_ptr->controls,
                                                       control);

        ASSERT_DEBUG_SYNC(result != ITEM_NOT_FOUND,
                          "Requested control was not found.");

        *(uint32_t*) out_result = result;
    }
    else
    if (property == UI_CONTROL_PROPERTY_GENERAL_TYPE)
    {
        *(_ui_control_type*) out_result = ui_control_ptr->control_type;
    }
    else
    if (ui_control_ptr->pfn_get_property_func_ptr != NULL)
    {
        /* UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED and
         * UI_CONTROL_PROPERTY_GENERLA_VISIBLE are handled
         * exclusively by each control */
        ui_control_ptr->pfn_get_property_func_ptr(ui_control_ptr->internal,
                                                  property,
                                                  out_result);
    }
}

/** Please see header for specification */
PUBLIC ral_context ui_get_context(ui ui_instance)
{
    return ogl_text_get_context( ((_ui*) ui_instance)->text_renderer);
}

/** Please see header for specification */
PUBLIC ral_program ui_get_registered_program(ui                        ui_instance,
                                             system_hashed_ansi_string name)
{
    ral_program result = NULL;
    _ui*        ui_ptr = (_ui*) ui_instance;

    system_hash64map_get(ui_ptr->registered_ui_control_programs,
                         system_hashed_ansi_string_get_hash(name),
                        &result);

    if (result != NULL)
    {
        ral_context_retain_object(ogl_text_get_context(ui_ptr->text_renderer),
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  result);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ui_lock(ui                                  ui_instance,
                                system_read_write_mutex_access_type access_type)
{
    system_read_write_mutex_lock( ((_ui*) ui_instance)->controls_rw_mutex,
                                  access_type);
}

/** Please see header for specification */
PUBLIC void ui_receive_control_callback(ui         ui_instance,
                                        ui_control control,
                                        int        callback_id,
                                        void*      callback_user_arg)
{
    system_resizable_vector callback_vector = NULL;
    _ui*                    ui_ptr          = (_ui*) ui_instance;

    /* Check if there is any callback vector associated with the control */
    if (system_hash64map_get(ui_ptr->registered_ui_control_callbacks,
                             (system_hash64) control,
                            &callback_vector))
    {
        unsigned int n_callbacks = 0;

        system_resizable_vector_get_property(callback_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_callbacks);

        for (unsigned int n_callback = 0;
                          n_callback < n_callbacks;
                        ++n_callback)
        {
            _ui_callback* callback_ptr = NULL;

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
PUBLIC EMERALD_API void ui_register_control_callback(ui                        ui,
                                                     ui_control                control,
                                                     int                       callback_id,
                                                     PFNUIEVENTCALLBACKPROCPTR callback_proc_ptr,
                                                     void*                     callback_proc_user_arg)
{
    system_resizable_vector callback_vector      = NULL;
    void*                   internal_control_ptr = _ui_get_internal_control_ptr(control);
    _ui*                    ui_ptr               = (_ui*) ui;

    /* If there's no callback vector associated with the control, spawn one now */
    if (!system_hash64map_get(ui_ptr->registered_ui_control_callbacks,
                              (system_hash64) internal_control_ptr,
                             &callback_vector) )
    {
        callback_vector = system_resizable_vector_create(4 /* capacity */);

        ASSERT_ALWAYS_SYNC(callback_vector != NULL,
                           "Could not create a callback vector");

        system_hash64map_insert(ui_ptr->registered_ui_control_callbacks,
                                (system_hash64) internal_control_ptr,
                                callback_vector,
                                NULL,  /* on_remove_callback_proc */
                                NULL); /* on_remove_callback_proc_user_arg */
    }

    /* Spawn the callback descriptor */
    _ui_callback* new_callback_ptr = new (std::nothrow) _ui_callback;

    ASSERT_ALWAYS_SYNC(new_callback_ptr != NULL,
                       "Out of memory");
    if (new_callback_ptr != NULL)
    {
        new_callback_ptr->callback_id            = callback_id;
        new_callback_ptr->callback_proc_ptr      = callback_proc_ptr;
        new_callback_ptr->callback_proc_user_arg = callback_proc_user_arg;

        /* Store it */
        system_resizable_vector_push(callback_vector,
                                     new_callback_ptr);
    }
}

/** Please see header for specification */
PUBLIC bool ui_register_program(ui                        ui_instance,
                                system_hashed_ansi_string program_name,
                                ral_program               program)
{
    const system_hash64 program_name_hash = system_hashed_ansi_string_get_hash(program_name);
    bool                result            = false;
    _ui*                ui_ptr            = (_ui*) ui_instance;

    if (!system_hash64map_contains(ui_ptr->registered_ui_control_programs,
                                   program_name_hash) )
    {
        system_hash64map_insert(ui_ptr->registered_ui_control_programs,
                                program_name_hash,
                                program,
                                NULL,
                                NULL);

        ral_context_retain_object(ogl_text_get_context(ui_ptr->text_renderer),
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  program);

        result = true;
    } /* if (!system_hash64map_contains(ui_ptr->registered_ui_control_programs, program_name_hash) ) */
    else
    {
        LOG_ERROR("UI program [%s] will not be registered - already stored",
                  system_hashed_ansi_string_get_buffer(program_name) );
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ui_reposition_control(ui_control   control,
                                              unsigned int new_control_index)
{
    _ui_control* control_ptr = (_ui_control*) control;
    unsigned int n_controls  = 0;
    _ui*         ui_ptr      = control_ptr->owner_ptr;

    system_resizable_vector_get_property(ui_ptr->controls,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_controls);

    if (n_controls > new_control_index)
    {
        LOG_ERROR("Performance warning: Code-path that could use some optimizations");

        /* Locate the requested control */
        unsigned int control_index = system_resizable_vector_find(ui_ptr->controls,
                                                                  control);

        ASSERT_DEBUG_SYNC(control_index != new_control_index,
                          "ui_reposition_control() call will NOP");

        if (control_index != new_control_index)
        {
            system_read_write_mutex_lock(ui_ptr->controls_rw_mutex,
                                         ACCESS_WRITE);
            {
                /* Insert the control in a new location */
                system_resizable_vector_delete_element_at(ui_ptr->controls,
                                                          control_index);

                if (new_control_index < control_index)
                {
                    /* Preceding controls have not had their indices changed */
                    system_resizable_vector_insert_element_at(ui_ptr->controls,
                                                              new_control_index,
                                                              control);
                }
                else
                {
                    /* Subsequent controls have had their indices decremented so take
                     * that into account */
                    ASSERT_DEBUG_SYNC(new_control_index > 0,
                                      "new_control_index <= 0 !");

                    system_resizable_vector_insert_element_at(ui_ptr->controls,
                                                              new_control_index - 1,
                                                              control);
                }
            }
            system_read_write_mutex_unlock(ui_ptr->controls_rw_mutex,
                                           ACCESS_WRITE);
        } /* if (control_index != new_control_index) */
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ui_set_control_property(ui_control          control,
                                                ui_control_property property,
                                                const void*         data)
{
    _ui_control* ui_control_ptr = (_ui_control*) control;

    if (ui_control_ptr == NULL)
    {
        return;
    }

    ASSERT_DEBUG_SYNC(ui_control_ptr->pfn_set_property_func_ptr != NULL,
                      "Requested user control does not support property setters");

    if (ui_control_ptr->pfn_set_property_func_ptr != NULL)
    {
        ui_control_ptr->pfn_set_property_func_ptr(ui_control_ptr->internal,
                                                  property,
                                                  data);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ui_unlock(ui                                  ui_instance,
                                  system_read_write_mutex_access_type access_type)
{
    system_read_write_mutex_unlock( ((_ui*) ui_instance)->controls_rw_mutex,
                                    access_type);
}