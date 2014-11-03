/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef SCENE_H
#define SCENE_H

#include "ogl/ogl_types.h"
#include "mesh/mesh_types.h"
#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene, scene)

enum scene_property
{
    SCENE_PROPERTY_FPS,                    /*     settable, float. By default set to 0 which
                                            *               disables scene graph computation's FPS
                                            *               limiter which is needed for correct LW
                                            *               animation playback. */
    SCENE_PROPERTY_GRAPH,                  /* not settable, scene_graph */
    SCENE_PROPERTY_MAX_ANIMATION_DURATION, /*     settable, float */
    SCENE_PROPERTY_N_CAMERAS,              /* not settable, uint32_t */
    SCENE_PROPERTY_N_LIGHTS,               /* not settable, uint32_t */
    SCENE_PROPERTY_N_MESH_INSTANCES,       /* not settable, uint32_t */
    SCENE_PROPERTY_NAME,                   /* not settable, system_hashed_ansi_string */
};

/** TODO */
PUBLIC EMERALD_API scene scene_create(__in __notnull ogl_context,
                                      __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API bool scene_add_camera(__in __notnull scene,
                                         __in __notnull scene_camera);

/** TODO */
PUBLIC EMERALD_API bool scene_add_curve(__in __notnull scene,
                                        __in __notnull scene_curve);

/** TODO */
PUBLIC EMERALD_API bool scene_add_light(__in __notnull scene,
                                        __in __notnull scene_light);

/** TODO */
PUBLIC EMERALD_API bool scene_add_mesh_instance(__in __notnull scene                     scene,
                                                __in __notnull mesh                      mesh_data,
                                                __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool scene_add_mesh_instance_defined(__in __notnull scene      scene,
                                                        __in __notnull scene_mesh mesh);

/** TODO. Sets texture id for given @param scene_texture instance.
 *
 **/
PUBLIC EMERALD_API bool scene_add_texture(__in __notnull scene,
                                          __in __notnull scene_texture);


/** TODO */
PUBLIC EMERALD_API scene_camera scene_get_camera_by_index(__in __notnull scene        scene,
                                                          __in           unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_camera scene_get_camera_by_name(__in __notnull scene                     scene,
                                                         __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_curve scene_get_curve_by_id(__in __notnull scene          instance,
                                                     __in           scene_curve_id id);

/** TODO */
PUBLIC EMERALD_API scene_light scene_get_light_by_index(__in __notnull scene        scene,
                                                        __in           unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_light scene_get_light_by_name(__in __notnull scene                     scene,
                                                       __in           system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_index(__in __notnull scene        scene,
                                                               __in           unsigned int index);

/** TODO */
PUBLIC EMERALD_API scene_mesh scene_get_mesh_instance_by_name(__in __notnull scene                     scene,
                                                              __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void scene_get_property(__in  __notnull scene          scene,
                                           __in            scene_property property,
                                           __out __notnull void*          out_result);

/** TODO */
PUBLIC EMERALD_API scene_texture scene_get_texture_by_name(__in __notnull scene                     instance,
                                                           __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene scene_load(__in __notnull ogl_context,
                                    __in __notnull system_hashed_ansi_string full_file_name_with_path);

/** TODO */
PUBLIC EMERALD_API scene scene_load_with_serializer(__in __notnull ogl_context,
                                                    __in __notnull system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API bool scene_save(__in __notnull scene                     instance,
                                   __in __notnull system_hashed_ansi_string full_file_name_with_path);

/** TODO */
PUBLIC EMERALD_API bool scene_save_with_serializer(__in __notnull scene                  instance,
                                                   __in __notnull system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API void scene_set_graph(__in __notnull scene       scene,
                                        __in __notnull scene_graph graph);

/** TODO */
PUBLIC EMERALD_API void scene_set_property(__in __notnull scene          scene,
                                           __in __notnull scene_property property,
                                           __in __notnull const void*    data);

#endif /* SCENE_H */
