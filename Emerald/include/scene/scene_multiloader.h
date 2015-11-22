/**
 *
 * Emerald (kbi/elude @2015)
 *
 * scene_multiloader is a tool which allows you to load multiple scenes in parallel,
 * each scene loaded in a separate thread.
 *
 * TODO: In the near future, this will be expanded, so that the scene loading process
 *       is actually carried out as a set of tasks, where each task can be assigned
 *       to any of the available worker threads.
 */
#ifndef SCENE_MULTILOADER_H
#define SCENE_MULTILOADER_H

#include "scene/scene_types.h"

/** TODO */
PUBLIC EMERALD_API scene_multiloader scene_multiloader_create_from_filenames(ral_context                      context,
                                                                             unsigned int                     n_scenes,
                                                                             const system_hashed_ansi_string* scene_filenames);

/** TODO */
PUBLIC EMERALD_API scene_multiloader scene_multiloader_create_from_system_file_serializers(ral_context                   context,
                                                                                           unsigned int                  n_scenes,
                                                                                           const system_file_serializer* scene_file_serializers,
                                                                                           bool                          free_serializers_at_release_time = false);

/** TODO.
 *
 *  @param out_result_scene Loaded scene.
 *                          Caller must retain the returned scene instance, prior to the
 *                          scene_multiloader_release() call, lest the object is released!
 */
PUBLIC EMERALD_API void scene_multiloader_get_loaded_scene(scene_multiloader loader,
                                                           unsigned int      n_scene,
                                                           scene*            out_result_scene);

/** Kicks off the scene loading process. The loading process is executed in the background,
 *  and the execution flow is returned to the caller as soon as the background threads are
 *  set in motion.
 *
 *  While the loading process is in process, it is illegal to attempt to release the multiloader.
 *
 *  To block until the loading process finishes, call scene_multiloader_wait_until_finished().
 *
 *  @param loader scene_multiloader to perform the loading operation for.
 */
PUBLIC EMERALD_API void scene_multiloader_load_async(scene_multiloader loader);

/** TODO */
PUBLIC EMERALD_API void scene_multiloader_release(scene_multiloader instance);

/** TODO */
PUBLIC EMERALD_API void scene_multiloader_wait_until_finished(scene_multiloader loader);

#endif /* SCENE_MULTILOADER_H */
