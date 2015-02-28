
/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "scene/scene.h"
#include "scene/scene_multiloader.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"


/* Private declarations */
typedef enum
{
    /* Multiloader created, loading not started */
    SCENE_MULTILOADER_STATE_CREATED,

    /* Scene loading in progress */
    SCENE_MULTILOADER_STATE_LOADING_IN_PROGRESS,

    /* All scenes have been loaded */
    SCENE_MULTILOADER_STATE_FINISHED

} _scene_multiloader_state;


typedef struct _scene_multiloader_scene
{
    system_hashed_ansi_string  filename;
    struct _scene_multiloader* loader_ptr;
    scene                      result_scene;

    _scene_multiloader_scene()
    {
        filename     = NULL;
        loader_ptr   = NULL;
        result_scene = NULL;
    }

    ~_scene_multiloader_scene()
    {
        if (result_scene != NULL)
        {
            scene_release(result_scene);

            result_scene = NULL;
        }
    }
} _scene_multiloader_scene;

typedef struct _scene_multiloader
{
    ogl_context              context;
    bool                     is_finished;
    system_event             loading_finished_event;
    volatile unsigned int    n_scenes_loaded;
    system_resizable_vector  scenes;   /* _scene_multiloader_scene* */
    _scene_multiloader_state state;

    _scene_multiloader()
    {
        context                = NULL;
        loading_finished_event = system_event_create(true,   /* manual_reset */
                                                     false); /* start_state */
        n_scenes_loaded        = 0;
        state                  = SCENE_MULTILOADER_STATE_CREATED;
        scenes                 = NULL;
    }

    ~_scene_multiloader()
    {
        ASSERT_DEBUG_SYNC(state != SCENE_MULTILOADER_STATE_LOADING_IN_PROGRESS,
                          "Scene loading process is in progress!");

        if (context != NULL)
        {
            ogl_context_release(context);

            context = NULL;
        } /* if (context != NULL) */

        if (loading_finished_event != NULL)
        {
            system_event_release(loading_finished_event);

            loading_finished_event = NULL;
        }

        if (scenes != NULL)
        {
            _scene_multiloader_scene* scene_ptr = NULL;

            while (system_resizable_vector_pop(scenes,
                                              &scene_ptr) )
            {
                delete scene_ptr;

                scene_ptr = NULL;
            }
            system_resizable_vector_release(scenes);

            scenes = NULL;
        } /* if (scenes != NULL) */
    }
} _scene_multiloader;


/** TODO */
PRIVATE volatile void _scene_multiloader_load_scene(system_thread_pool_callback_argument arg)
{
    _scene_multiloader_scene* scene_ptr = (_scene_multiloader_scene*) arg;
    const unsigned int        n_scenes  = system_resizable_vector_get_amount_of_elements(scene_ptr->loader_ptr->scenes);

    /* Load the scene */
    scene_ptr->result_scene = scene_load(scene_ptr->loader_ptr->context,
                                         scene_ptr->filename);

    /* Update the status */
    if (::InterlockedIncrement(&scene_ptr->loader_ptr->n_scenes_loaded) == n_scenes)
    {
        scene_ptr->loader_ptr->state = SCENE_MULTILOADER_STATE_FINISHED;

        system_event_set(scene_ptr->loader_ptr->loading_finished_event);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API scene_multiloader scene_multiloader_create(__in                  ogl_context                      context,
                                                              __in                  unsigned int                     n_scenes,
                                                              __in_ecount(n_scenes) const system_hashed_ansi_string* scene_filenames)
{
    _scene_multiloader* multiloader_ptr = new (std::nothrow) _scene_multiloader;

    ASSERT_DEBUG_SYNC(multiloader_ptr != NULL,
                      "Out of memory");

    if (multiloader_ptr != NULL)
    {
        multiloader_ptr->context = context;
        multiloader_ptr->scenes  = system_resizable_vector_create(n_scenes,
                                                                  sizeof(system_hashed_ansi_string) );

        ASSERT_DEBUG_SYNC(multiloader_ptr->scenes != NULL,
                          "Could not spawn scenes vector");

        /* Create a descriptor for each enqueued scene */
        for (unsigned int n_scene = 0;
                          n_scene < n_scenes;
                        ++n_scene)
        {
            _scene_multiloader_scene* scene_ptr = new (std::nothrow) _scene_multiloader_scene;

            ASSERT_DEBUG_SYNC(scene_ptr != NULL,
                              "Out of memory");

            scene_ptr->filename   = scene_filenames[n_scene];
            scene_ptr->loader_ptr = multiloader_ptr;

            system_resizable_vector_push(multiloader_ptr->scenes,
                                         scene_ptr);
        } /* for (all scenes to be enqueued) */

        /* Retain the context - we'll need it for the loading process. */
        ogl_context_retain(context);
    }

    return (scene_multiloader) multiloader_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_get_loaded_scene(__in  __notnull scene_multiloader loader,
                                                           __in            unsigned int      n_scene,
                                                           __out __notnull scene*            out_result_scene)
{
    _scene_multiloader* loader_ptr = (_scene_multiloader*) loader;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(loader_ptr != NULL,
                      "scene_multiloader instance is NULL");
    ASSERT_DEBUG_SYNC(loader_ptr->state == SCENE_MULTILOADER_STATE_FINISHED,
                      "Scene loading process is not finished yet!");
    ASSERT_DEBUG_SYNC(out_result_scene != NULL,
                      "Out argument is NULL");

    /* Retrieve the requested scene */
    const unsigned int n_scenes  = system_resizable_vector_get_amount_of_elements(loader_ptr->scenes);

    ASSERT_DEBUG_SYNC(n_scene < n_scenes,
                      "Invalid scene index requested");

    if (n_scenes > n_scene)
    {
        _scene_multiloader_scene* scene_ptr = NULL;

        system_resizable_vector_get_element_at(loader_ptr->scenes,
                                               n_scene,
                                              &scene_ptr);

        *out_result_scene = scene_ptr->result_scene;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_load_async(__in __notnull scene_multiloader loader)
{
    _scene_multiloader* instance_ptr = (_scene_multiloader*) loader;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(instance_ptr != NULL,
                      "scene_multiloader instance is NULL.");

    if (instance_ptr->state != SCENE_MULTILOADER_STATE_CREATED)
    {
        ASSERT_DEBUG_SYNC(false,
                          "scene_multiloader_load_async() failed: scene_multiloader() has already been used to load scenes.");

        goto end;
    }

    /* Use system thread pool to load the scene.
     *
     * NOTE: This will break in non-deterministic way if any of the scene assets are shared between scenes!
     *       The implementation will shortly be changed to make the multiloader work for such cases as well.
     */
    const unsigned int n_scenes = system_resizable_vector_get_amount_of_elements(instance_ptr->scenes);

    for (unsigned int n_scene = 0;
                      n_scene < n_scenes;
                    ++n_scene)
    {
        _scene_multiloader_scene* scene_ptr = NULL;

        system_resizable_vector_get_element_at(instance_ptr->scenes,
                                               n_scene,
                                              &scene_ptr);

        ASSERT_DEBUG_SYNC(scene_ptr != NULL,
                          "Could not retrieve scene descriptor at index [%d]",
                          n_scene);

        system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                         _scene_multiloader_load_scene,
                                                                                                         scene_ptr);

        system_thread_pool_submit_single_task(task);
    } /* for (all enqueued scenes) */

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_release(__in __notnull __post_invalid scene_multiloader instance)
{
    _scene_multiloader* instance_ptr = (_scene_multiloader*) instance;

    if (instance_ptr->context != NULL)
    {
        ogl_context_release(instance_ptr->context);

        instance_ptr->context = NULL;
    }

    delete instance_ptr;
    instance_ptr = NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API void scene_multiloader_wait_until_finished(__in __notnull scene_multiloader loader)
{
    _scene_multiloader* loader_ptr = (_scene_multiloader*) loader;

    system_event_wait_single_infinite(loader_ptr->loading_finished_event);
}