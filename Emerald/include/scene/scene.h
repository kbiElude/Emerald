/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SCENE_H
#define SCENE_H

#include "curve/curve_types.h"
#include "ogl/ogl_types.h"
#include "mesh/mesh_types.h"
#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene, 
                             scene)

enum scene_property
{
    SCENE_PROPERTY_CALLBACK_MANAGER,       /* not settable, system_callback_manager. */
    SCENE_PROPERTY_FPS,                    /*     settable, float. By default set to 0 which
                                            *               disables scene graph computation's FPS
                                            *               limiter which is needed for correct LW
                                            *               animation playback. */
    SCENE_PROPERTY_GRAPH,                  /* not settable, scene_graph */
    SCENE_PROPERTY_MAX_ANIMATION_DURATION, /*     settable, float */
    SCENE_PROPERTY_N_CAMERAS,              /* not settable, uint32_t */
    SCENE_PROPERTY_N_LIGHTS,               /* not settable, uint32_t */
    SCENE_PROPERTY_N_MATERIALS,            /* not settable, uint32_t */
    SCENE_PROPERTY_N_MESH_INSTANCES,       /* not settable, uint32_t */
    SCENE_PROPERTY_N_UNIQUE_MESHES,        /* not settable, uint32_t */
    SCENE_PROPERTY_NAME,                   /* not settable, system_hashed_ansi_string */

    SCENE_PROPERTY_SHADOW_MAPPING_ENABLED, /*     settable, bool. Set in run-time.
                                            *
                                            * Tells if shadow mapping should be enabled for lights
                                            * which are marked as shadow caster. This is used by
                                            * material manager to determine which materials should be
                                            * returned.
                                            */
};

typedef enum
{
    SCENE_CALLBACK_ID_LIGHT_ADDED, /* new_light_added; callback_proc_data: new scene_light */

    /* Always last */
    SCENE_CALLBACK_ID_COUNT
} scene_callback_id;

/** TODO */
PUBLIC EMERALD_API scene scene_create(ogl_context               context,
                                      system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool scene_add_camera(scene        scene_instance,
                                         scene_camera new_camera);

/** TODO */
PUBLIC EMERALD_API bool scene_add_curve(scene       scene_instance,
                                        scene_curve curve_instance);

/** TODO */
PUBLIC EMERALD_API bool scene_add_light(scene       scene_instance,
                                        scene_light light_instance);

/** TODO */
PUBLIC EMERALD_API bool scene_add_material(scene          scene_instance,
                                           scene_material material);

/** TODO.
 *
 *  NOTE: Object will be released at scene tear-down time. Make sure to retain the object if
 *        you know you're going to need it after the time.
 **/
PUBLIC EMERALD_API bool scene_add_mesh_instance(scene                     scene,
                                                mesh                      mesh_data,
                                                system_hashed_ansi_string name,
                                                scene_mesh*               out_opt_result_mesh_ptr = NULL);

/** TODO */
PUBLIC EMERALD_API bool scene_add_mesh_instance_defined(scene      scene,
                                                        scene_mesh mesh);

/** TODO. Sets texture id for given @param scene_texture instance.
 *
 **/
PUBLIC EMERALD_API bool scene_add_texture(scene         scene_instance,
                                          scene_texture texture_instance);


/** TODO */
PUBLIC EMERALD_API scene_camera scene_get_camera_by_index(scene        scene,
                                                          unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_camera scene_get_camera_by_name(scene                     scene,
                                                         system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_curve scene_get_curve_by_container(scene           instance,
                                                            curve_container curve);

/** TODO */
PUBLIC EMERALD_API scene_curve scene_get_curve_by_id(scene          instance,
                                                     scene_curve_id id);

/** TODO */
PUBLIC EMERALD_API scene_light scene_get_light_by_index(scene        scene,
                                                        unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_light scene_get_light_by_name(scene                     scene,
                                                       system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_material scene_get_material_by_index(scene        scene,
                                                              unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_material scene_get_material_by_name(scene                     scene,
                                                             system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_index(scene        scene,
                                                               unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_name(scene                     scene,
                                                              system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void scene_get_property(scene          scene,
                                           scene_property property,
                                           void*          out_result);

/** TODO */
PUBLIC EMERALD_API scene_texture scene_get_texture_by_index(scene        scene,
                                                            unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_texture scene_get_texture_by_name(scene                     instance,
                                                           system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API mesh scene_get_unique_mesh_by_index(scene        scene,
                                                       unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene scene_load(ogl_context               context,
                                    system_hashed_ansi_string full_file_name_with_path);

/** TODO */
PUBLIC EMERALD_API scene scene_load_with_serializer(ogl_context            context,
                                                    system_file_serializer serializer);

/** TODO.
 *
 *  @param curve_container_to_curve_id_map If not NULL, curve IDs taken from the map will be saved instead
 *                                         of the containers.
 **/
PUBLIC EMERALD_API bool scene_save(scene                     instance,
                                   system_hashed_ansi_string full_file_name_with_path);

/** TODO */
PUBLIC EMERALD_API bool scene_save_with_serializer(scene                  instance,
                                                   system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API void scene_set_graph(scene       scene,
                                        scene_graph graph);

/** TODO */
PUBLIC EMERALD_API void scene_set_property(scene          scene,
                                           scene_property property,
                                           const void*    data);

#endif /* SCENE_H */
