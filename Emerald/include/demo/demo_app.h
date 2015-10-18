/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef DEMO_APP_H
#define DEMO_APP_H

#include "demo/demo_types.h"

typedef enum
{

    /* ogl_context_type, settable (before calling run() ).
     *
     * Defines type of the rendering context to set up for the demo.
     *
     * Default value: OGL_CONTEXT_TYPE_GL
     **/
    DEMO_APP_PROPERTY_CONTEXT_TYPE,

    /* bool, settable (before calling run() ).
     *
     * Defines whether the demo should run in full-screen or not.
     *
     * Default value: false
     **/
    DEMO_APP_PROPERTY_FULLSCREEN,

    /* demo_loader, not settable.
     *
     * Loader object that will be used to initialize demo contents.
     **/
    DEMO_APP_PROPERTY_LOADER,

    /* uint32_t, settable (before calling run() ).
     *
     * Refresh rate to use for the full-screen mode.
     *
     * NOTE: This value is separate from target frame rate used to control the frequency, at which frame contents
     *       is going to be updated! The two values are recommended to, but not necessarily have to match.
     *
     * Default value: 60
     **/
     DEMO_APP_PROPERTY_REFRESH_RATE,

    /* uint32_t[2], settable (before calling run() ).
     *
     * Defines target window or full-screen resolution.
     *
     * Default value: 1280x720.
     **/
    DEMO_APP_PROPERTY_RESOLUTION,

    /* uint32_t, settable (before calling run() ).
     *
     * Target FPS to use for rendering.
     *
     * NOTE: This value is separate from the refresh rate used for setting up the full-screen mode! The two values
     *       are recommended to, but not necessarily have to match.
     *
     * Default value: 60
     **/
    DEMO_APP_PROPERTY_TARGET_FRAME_RATE,

    /* bool, settable (before calling run() ).
     *
     * Tells if (adaptive) vertical sync should be enabled.
     *
     * Default value: true.
     */
    DEMO_APP_PROPERTY_USE_VSYNC
} demo_app_property;


/** Creates a new demo application instance.
 *
 *  After calling this function, the client should retrieve the loader instance and configure it
 *  accordingly (the FULLSCREEN, RESOLUTION properties, in specific). The demo app
 *  should also enqueue at least one command to receive a callback, allowing it to configure the
 *  timeline content. This has been made a loader command, so that the window contents can be
 *  updated, while the initialization is happening in the background.
 *
 *  If the loader loads an audio stream, that stream will be used for audio playback. If more than one audio
 *  stream is initialized, only the first one will be used.
 *
 *  After loader is initialized, the client should call demo_app_run(). A rendering window will then
 *  be created, the rendering context will be initialized, and loader will begin executing the
 *  enqueued commands (potentially in a multithreaded manner <in the future>).
 *
 *  After the initialization process finishes, demo playback starts. The demo_app takes care of watching
 *  for key-strokes & making sure the demo closes once the audio stream finishes.
 *
 *  @param name Demo name. This will also be used for the window title.
 */
PUBLIC EMERALD_API demo_app demo_app_create(system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void demo_app_get_property(const demo_app    app,
                                              demo_app_property property,
                                              void*             out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void demo_app_release(demo_app app);

/** TODO */
PUBLIC EMERALD_API void demo_app_run(demo_app app);

/** TODO */
PUBLIC EMERALD_API void demo_app_set_property(demo_app          app,
                                              demo_app_property property,
                                              const void*       data_ptr);

#endif /* DEMO_APP_H */
