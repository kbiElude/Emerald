#ifndef DEMO_WINDOW_H
#define DEMO_WINDOW_H

#include "demo/demo_types.h"

typedef enum
{
    /* not settable; available after show() call; system_window */
    DEMO_WINDOW_PRIVATE_PROPERTY_WINDOW,

} demo_window_private_property;

typedef enum
{
    /* not settable; ral_backend_type */
    DEMO_WINDOW_PROPERTY_BACKEND_TYPE,

    /* not settable; bool */
    DEMO_WINDOW_PROPERTY_FULLSCREEN,

    /* not settable; demo_flyby */
    DEMO_WINDOW_PROPERTY_FLYBY,

    /* not settable; PFNDEMOWINDOWLOADERSETUPCALLBACKPROC */
    DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_FUNC_PTR,

    /* not settable; void* */
    DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_USER_ARG,

    /* not settable; system_hashed_ansi_string */
    DEMO_WINDOW_PROPERTY_NAME,

    /* not settable; uint32_t */
    DEMO_WINDOW_PROPERTY_REFRESH_RATE,

    /* not settable; ral_context */
    DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,

    /* not settable; ral_rendering_handler */
    DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,

    /* not settable; uint32_t[2] */
    DEMO_WINDOW_PROPERTY_RESOLUTION,

    /* not settable; uint32_t **/
    DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE,

    /* not settable; demo_timeline. Available after the window is shown. */
    DEMO_WINDOW_PROPERTY_TIMELINE,

    /* not settable; bool */
    DEMO_WINDOW_PROPERTY_USE_VSYNC,

    /* settable; bool */
    DEMO_WINDOW_PROPERTY_VISIBLE,
} demo_window_property;

typedef void (*PFNDEMOWINDOWLOADERSETUPCALLBACKPROC)(demo_window window,
                                                     demo_loader loader,
                                                     void*       user_arg);

typedef struct demo_window_create_info
{
    bool     fullscreen;
    uint32_t refresh_rate;
    uint32_t resolution[2];
    bool     use_vsync;
    bool     visible;

    /* Setting this to 0 will cause the window to render one frame per each start_rendering() request.
     * Setting this to ~0 will cause the window to render as many frames as possible. */
    uint32_t target_rate;

    void*                                loader_callback_func_user_arg;
    PFNDEMOWINDOWLOADERSETUPCALLBACKPROC pfn_loader_callback_func_ptr;

    demo_window_create_info()
    {
        fullscreen                    = false;
        loader_callback_func_user_arg = NULL;
        pfn_loader_callback_func_ptr  = NULL;
        refresh_rate                  = 60;
        resolution[0]                 = 1280;
        resolution[1]                 = 720;
        target_rate                   = 60;
        use_vsync                     = true;
        visible                       = true;
    }
} demo_window_create_info;


/** TODO */
PUBLIC EMERALD_API bool demo_window_add_callback_func(demo_window                          window,
                                                      system_window_callback_func_priority priority,
                                                      system_window_callback_func          callback_func,
                                                      void*                                pfn_callback_func,
                                                      void*                                callback_func_user_arg);

/** TODO
 *
 *  Releases the underlying rendering handler, as well as the system window instance.
 *  It is OK to call demo_window_show() for the same demo_window instance afterward.
 *
 *  This call should only be issued by demo_app.
 */
PUBLIC bool demo_window_close(demo_window window);

/** TODO.
 *
 *  Internal only. Use demo_app to access this functionality.
 *
 *  After calling this function, the client should configure the window accordingly (the FULLSCREEN,
 *  RESOLUTION properties, in specific). The demo app should also adjust the loader set-up callbacks,
 *  if there are any assets required. The specified entry-point will be called during window initialization
 *  time, so that the client can enqueue one or more commands to load any meshes/scenes/textures needed,
 *  as well as set up timeline contents.
 *
 *  If the loader loads an audio stream, that stream will be used for audio playback. If more than one audio
 *  stream is initialized, only the first one will be used.
 *
 *  After loader is initialized and timeline is set up, a rendering window will be created, the rendering
 *  backend will be initialized, and loader will begin executing the enqueued commands (potentially in
 *  a multithreaded manner <in the future>).
 *
 *  After the initialization process finishes, demo playback starts. The demo_window takes care of watching
 *  for key-strokes & making sure the window closes once the audio stream finishes.
 *
 *  @param name         Demo name. This will also be used for the window title.
 *  @param create_info  Properties of the window to be created.
 *  @param backend_type Type of the rendering backend to use.
 *  @param use_timeline True if a timeline should be instantiated for the window. This means that any
 *                      custom rendering entry-point you assign to the rendering handler will be ignored.
 *                      However, timeline is generally very useful for more complex demo applications.
 */
PUBLIC demo_window demo_window_create(system_hashed_ansi_string      name,
                                      const demo_window_create_info& create_info,
                                      ral_backend_type               backend_type,
                                      bool                           use_timeline);

/** TODO */
PUBLIC void demo_window_get_private_property(const demo_window            window,
                                             demo_window_private_property property,
                                             void*                        out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void demo_window_get_property(const demo_window    window,
                                                 demo_window_property property,
                                                 void*                out_result_ptr);

/** TODO
 *
 *  Internal only. Use demo_app to access this functionality.
 **/
PUBLIC void demo_window_release(demo_window window);

/** TODO
 *
 **/
PUBLIC EMERALD_API bool demo_window_start_rendering(demo_window window,
                                                    system_time rendering_start_time);

/** TODO */
PUBLIC EMERALD_API bool demo_window_stop_rendering(demo_window window);

/** TODO */
PUBLIC EMERALD_API bool demo_window_wait_until_closed(demo_window window);

#endif /* DEMO_WINDOW_H */