/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef OGL_SCENE_RENDERER_H
#define OGL_SCENE_RENDERER_H

#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

typedef enum
{
    HELPER_VISUALIZATION_NONE           = 0,
    HELPER_VISUALIZATION_BOUNDING_BOXES = 1 << 0,
    HELPER_VISUALIZATION_NORMALS        = 1 << 1,
} _ogl_scene_renderer_helper_visualization;

typedef enum
{
    RENDER_MODE_REGULAR,
    RENDER_MODE_AMBIENT_UV_ONLY,
    RENDER_MODE_DIFFUSE_UV_ONLY,
    RENDER_MODE_SPECULAR_UV_ONLY,
    RENDER_MODE_NORMALS_ONLY,
} _ogl_scene_renderer_render_mode;

typedef enum
{
    OGL_SCENE_RENDERER_PROPERTY_GRAPH,              /*         property,                 value: scene_graph */
    OGL_SCENE_RENDERER_PROPERTY_MESH_INSTANCE,      /* indexed property, key: mesh id // value: mesh */
    OGL_SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX,  /* indexed property, key: mesh id // value: system_matrix4x4 */
    OGL_SCENE_RENDERER_PROPERTY_MESH_NORMAL_MATRIX, /* indexed property, key: mesh id // value: system_matrix4x4 */
} ogl_scene_renderer_property;

/** TODO. **/
PUBLIC EMERALD_API ogl_scene_renderer ogl_scene_renderer_create(__in __notnull ogl_context context,
                                                                __in __notnull scene       scene);

/** TODO. Internal usage only */
PUBLIC void ogl_scene_renderer_get_indexed_property(__in  __notnull const ogl_scene_renderer    renderer,
                                                    __in            ogl_scene_renderer_property property,
                                                    __in            uint32_t                    index,
                                                    __out __notnull void*                       out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_scene_renderer_get_property(__in  __notnull ogl_scene_renderer          renderer,
                                                        __in            ogl_scene_renderer_property property,
                                                        __out __notnull void*                       out_result);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void ogl_scene_renderer_render_scene_graph(__in           __notnull ogl_scene_renderer                       renderer,
                                                                                     __in           __notnull system_matrix4x4                         view,
                                                                                     __in           __notnull system_matrix4x4                         projection,
                                                                                     __in_ecount(3) __notnull const float*                             camera_location,
                                                                                     __in                     const _ogl_scene_renderer_render_mode&   render_mode,
                                                                                     __in           __notnull _ogl_scene_renderer_helper_visualization helper_visualization,
                                                                                     __in                     system_timeline_time                     frame_time);

/** TODO. **/
PUBLIC EMERALD_API void ogl_scene_renderer_release(__in __notnull ogl_scene_renderer renderer);

#endif /* OGL_SCENE_RENDERER_H */