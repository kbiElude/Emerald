/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef DEMO_LOADER_H
#define DEMO_LOADER_H

#include "demo/demo_timeline.h"
#include "ogl/ogl_types.h"
#include "system/system_types.h"

typedef void (*PFNBUILDPROGRAMCALLBACKPROC)     (ogl_program   result_program,
                                                 void*         user_arg);
typedef void (*PFNCONFIGURETIMELINECALLBACKPROC)(demo_timeline timeline,
                                                 void*         user_arg);
typedef void (*PFNLOADERTEARDOWNCALLBACKPROC)   (void*         user_arg);

typedef enum
{
    DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM,
    DEMO_LOADER_OBJECT_TYPE_PROGRAM,
    DEMO_LOADER_OBJECT_TYPE_SCENE,

    /* TODO: Add other object types */

    /* Always last */
    DEMO_LOADER_OBJECT_TYPE_COUNT
} demo_loader_object_type;

typedef enum
{
    /* Builds a program object from user-specified GLSL shaders.
     *
     * operation_arg = ptr to demo_loader_op_build_program structure instance.
     */
    DEMO_LOADER_OP_BUILD_PROGRAM,

    /* Calls back user-provided function pointer, so that the client can configure the
     * timeline contents.
     *
     * operation_arg = ptr to demo_loader_op_configure_timeline structure instance
    */
    DEMO_LOADER_OP_CONFIGURE_TIMELINE,

    /* Loads an audio stream from user-specified location.
     *
     * operation_arg = ptr to demo_loader_op_load_audio_stream structure instance
     */
    DEMO_LOADER_OP_LOAD_AUDIO_STREAM,

    /* Loads one or more scenes from user-specified location.
     *
     * operation_arg = ptr to demo_loader_op_load_scenes structure instance.
     */
    DEMO_LOADER_OP_LOAD_SCENES,

    /* Starts rendering playback. The rendering handler is configured to call user-specified
     * handler every frame.
     * It is caller's responsibility to stop the playback when it is no longer needed.
     *
     * operation_arg = ptr to demo_loader_op_render_animation structure instance.
     */
    DEMO_LOADER_OP_RENDER_ANIMATION,

    /* Renders a single frame and shows it on screen. The rendering handler is configured to
     * call user-specified handler to render frame contents.
     *
     * operation_arg = ptr to demo_loader_op_render_frame structure instance.
     */
    DEMO_LOADER_OP_RENDER_FRAME,

    /* Calls back the specified func ptr from a rendering thread. No buffer swap will occur
     * after the call is returned from.
     *
     * operation_arg = ptr to demo_loader_op_request_rendering_thread_callback structure instance.
     */
    DEMO_LOADER_OP_REQUEST_RENDERING_THREAD_CALLBACK
} demo_loader_op;

typedef struct
{
    system_hashed_ansi_string name;

    const char*               body;

    uint32_t                   n_tokens;
    system_hashed_ansi_string* token_keys;
    system_hashed_ansi_string* token_values;
} demo_loader_op_build_program_shader;

typedef struct
{
    /* Compute shader descriptor. If cs::body != NULL, ::body for all other shader stages must be NULL */
    demo_loader_op_build_program_shader cs;

    /* Fragment shader descriptor. If fs::body != NULL, cs::body must be NULL */
    demo_loader_op_build_program_shader fs;

    /* Geometry shader descriptor. If gs::body != NULL, cs::body must be NULL */
    demo_loader_op_build_program_shader gs;

    /* Tessellation control shader descriptor. If tc::body != NULL, cs::body must be NULL */
    demo_loader_op_build_program_shader tc;

    /* Tessellation evaluation shader descriptor. If te::body != NULL, cs::body must be NULL */
    demo_loader_op_build_program_shader te;

    /* Vertex shader descriptor. If vs::body != NULL, cs::body must be NULL */
    demo_loader_op_build_program_shader vs;

    /* Func pointer which will be called from the loader's thread after ogl_program links successfully */
    PFNBUILDPROGRAMCALLBACKPROC pfn_on_link_finished_callback_proc;
    void*                       on_link_finished_callback_user_arg;

    /* Optional func pointer which will be called right before the demo application quits. The call-back
     * is not going to be coming from the rendering thread but the rendering context is still going to be
     * available, in case you need it.
     */
    PFNLOADERTEARDOWNCALLBACKPROC pfn_teardown_func_ptr;
    void*                         teardown_callback_user_arg;

    system_hashed_ansi_string name;
    uint32_t*                 result_program_index_ptr; /* will hold index of the created ogl_program instance */
} demo_loader_op_build_program;

typedef struct
{
    /* Function pointer to call with the demo_timeline instance passed as an argument */
    PFNCONFIGURETIMELINECALLBACKPROC pfn_configure_timeline_proc;

    /* Optional func pointer which will be called right before the demo application quits. The call-back
     * is not going to be coming from the rendering thread but the rendering context is still going to be
     * available, in case you need it.
     */
    PFNLOADERTEARDOWNCALLBACKPROC pfn_teardown_func_ptr;
    void*                         teardown_callback_user_arg;

    /* User argument to pass along the call-back */
    void*                            user_arg;
} demo_loader_op_configure_timeline;

typedef struct
{
    /* TODO: packed file support */
    system_hashed_ansi_string audio_file_name;
    uint32_t*                 result_audio_stream_index_ptr; /* will hold index of the loaded audio stream instance. can be NULL. */

    /* Optional func pointer which will be called right before the demo application quits. The call-back
     * is not going to be coming from the rendering thread but the rendering context is still going to be
     * available, in case you need it.
     */
    PFNLOADERTEARDOWNCALLBACKPROC pfn_teardown_func_ptr;
    void*                         teardown_callback_user_arg;
} demo_loader_op_load_audio_stream;

typedef struct
{
    /** TODO: packed file support */
    uint32_t                   n_scenes;
    uint32_t*                  result_scene_indices_ptr; /* will hold index/indices of the loaded scene instances. must be user-preallocated and able to hold at least n_scenes uints */
    system_hashed_ansi_string* scene_file_names;         /* holds n_scenes names */
} demo_loader_op_load_scenes;

typedef struct
{
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback_proc;
    void*                                   user_arg;
} demo_loader_op_render_animation;

typedef struct
{
    PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_rendering_callback_proc;
    void*                                      user_arg;
} demo_loader_op_render_frame;

typedef struct
{
    PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_rendering_callback_proc;
    bool                                       should_swap_buffers;
    void*                                      user_arg;

    /* Optional func pointer which will be called right before the demo application quits. The call-back
     * is not going to be coming from the rendering thread but the rendering context is still going to be
     * available, in case you need it.
     */
    PFNLOADERTEARDOWNCALLBACKPROC pfn_teardown_func_ptr;
    void*                         teardown_callback_user_arg;
} demo_loader_op_request_rendering_thread_callback;

typedef enum
{
    /* uint32_t, not settable.
     *
     * Number of audio_stream instances loaded. Should be queried after _run() returns.
     *
     **/
    DEMO_LOADER_PROPERTY_N_LOADED_AUDIO_STREAMS,

} demo_loader_property;



/** TODO */
PUBLIC demo_loader demo_loader_create(ral_context context);

/** Enqueues an operation for execution at _run() time. The operations will be executed
 *  in the order of operation submissions.
 *
 *  @param loader        Demo loader instance.
 *  @param operation     Type of operation to enqueue
 *  @param operation_arg Pointer to the operation-specific argument structure. Refer to docs for more details.
 *                       The data under @param operation_arg will be cached internally, so you can free the
 *                       memory after the call returns.
 **/
PUBLIC EMERALD_API void demo_loader_enqueue_operation(demo_loader    loader,
                                                      demo_loader_op operation,
                                                      void*          operation_arg);

/** TODO */
PUBLIC EMERALD_API bool demo_loader_get_object_by_index(demo_loader             loader,
                                                        demo_loader_object_type object_type,
                                                        uint32_t                index,
                                                        void*                   out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void demo_loader_get_property(demo_loader          loader,
                                                 demo_loader_property property,
                                                 void*                out_data_ptr);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void demo_loader_release(demo_loader loader);

/** TODO */
PUBLIC void demo_loader_release_object_by_index(demo_loader             loader,
                                                demo_loader_object_type object_type,
                                                uint32_t                index);

/** TODO */
PUBLIC void demo_loader_run(demo_loader   loader,
                            demo_timeline timeline,
                            system_window renderer_window);

#endif /* DEMO_LOADER_H */
