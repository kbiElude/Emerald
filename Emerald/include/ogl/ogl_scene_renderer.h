/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef OGL_SCENE_RENDERER_H
#define OGL_SCENE_RENDERER_H

#include "mesh/mesh_types.h"
#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

#ifdef __linux__
    #include <cfloat>
#endif


#define DEFAULT_AABB_MAX_VALUE (FLT_MIN)
#define DEFAULT_AABB_MIN_VALUE (FLT_MAX)

typedef enum
{
    /* Passes all objects that are located in front of the current camera.
     *
     * behavior_data parameter: [0]..[2]: world-space location of the camera
     *                          [3]..[5]: normalized view direction vector.
     *
     * This behavior is required for DPSM shadow map generation passes, where
     * there is no projection matrix available (the projection is done directly
     * in VS)
     */
    OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_PASS_OBJECTS_IN_FRONT_OF_CAMERA,

    /* Extracts clipping planes from current projection matrix and checks
     * if the pointed mesh intersects or is fully embedded within the frustum.
     *
     * behavior_data parameter: ignored
     *
     * Recommended culling behavior, as of now.
     */
    OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES,

    OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_UNKNOWN
} _ogl_scene_renderer_frustum_culling_behavior;
typedef enum
{
    HELPER_VISUALIZATION_NONE           = 0,
    HELPER_VISUALIZATION_BOUNDING_BOXES = 1 << 0,
    HELPER_VISUALIZATION_FRUSTUMS       = 1 << 3,
    HELPER_VISUALIZATION_NORMALS        = 1 << 1,
    HELPER_VISUALIZATION_LIGHTS         = 1 << 2,
} _ogl_scene_renderer_helper_visualization;

typedef enum
{
    /* All meshes will be forward-rendered */
    RENDER_MODE_FORWARD_WITH_DEPTH_PREPASS,
    RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,

    /* TODO: RENDER_MODE_DEFERRED */

    /* TODO: RENDER_MODE_INFERRED? */

    /* (debug) All meshes will be shaded with their normal data */
    RENDER_MODE_NORMALS_ONLY,
    /* (debug) All meshes will be shaded with their texcoord data */
    RENDER_MODE_TEXCOORDS_ONLY,

    /* Crucial parts of the rendering process are handed over to
     * ogl_shadow_mapping. */
    RENDER_MODE_SHADOW_MAP,

} _ogl_scene_renderer_render_mode;

typedef enum
{
    /* general property, value: ogl_context */
    OGL_SCENE_RENDERER_PROPERTY_CONTEXT,

    /* general property, value: scene_graph */
    OGL_SCENE_RENDERER_PROPERTY_GRAPH,

    /* indexed property, key: mesh id // value: mesh */
    OGL_SCENE_RENDERER_PROPERTY_MESH_INSTANCE,

    /* indexed property, key: mesh id // value: system_matrix4x4.
     *
     * May not return any value if the model was frustum-culled out.
     */
    OGL_SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX,

    /* indexed property, key: mesh id // value: system_matrix4x4 */
    OGL_SCENE_RENDERER_PROPERTY_MESH_NORMAL_MATRIX,

    /* general property, value: float[3] */
    OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX,

    /* general property, value: float[3] */
    OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN,

    /* general property, value: system_matrix4x4 */
    OGL_SCENE_RENDERER_PROPERTY_VP,
} ogl_scene_renderer_property;


/** TODO.
 *
 *  Bakes in advance all GPU assets required to render a given scene.
 *
 */
PUBLIC EMERALD_API void ogl_scene_renderer_bake_gpu_assets(ogl_scene_renderer renderer);

/** TODO.
 *
 *  OGL_SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES behavior implementation
 *  is based on http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes-ii/
 *  and http://gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf.
 *
 *  This function also adjusts ogl_scene_renderer's max & min AABB coordinates.
 *
 *  @param renderer      TODO
 *  @param mesh_gpu      TODO
 *  @param behavior      TODO
 *  @param behavior_data Additional behavior-specific argument. Please see documentation of
 *                       _ogl_scene_renderer_frustum_culling_behavior for more details.
 *
 *  @return true if the mesh is inside OR intersects the frustum, false otherwise.
 *
 *  Note: if "true" is returned, OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX and
 *        OGL_SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN properties of @param renderer
 *        are also adjusted.
 */
PUBLIC bool ogl_scene_renderer_cull_against_frustum(ogl_scene_renderer                           renderer,
                                                    mesh                                         mesh_gpu,
                                                    _ogl_scene_renderer_frustum_culling_behavior behavior,
                                                    const void*                                  behavior_data);

/** TODO. **/
PUBLIC EMERALD_API ogl_scene_renderer ogl_scene_renderer_create(ogl_context context,
                                                                scene       scene);

/** TODO. Internal usage only */
PUBLIC void ogl_scene_renderer_get_indexed_property(const ogl_scene_renderer    renderer,
                                                    ogl_scene_renderer_property property,
                                                    uint32_t                    index,
                                                    void*                       out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_scene_renderer_get_property(ogl_scene_renderer          renderer,
                                                        ogl_scene_renderer_property property,
                                                        void*                       out_result);

/** TODO.
 *
 *  TODO: state-ify render_mode / shadow_mapping_type / helper_visualization arguments..
 **/
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void ogl_scene_renderer_render_scene_graph(ogl_scene_renderer                       renderer,
                                                                                     system_matrix4x4                         view,
                                                                                     system_matrix4x4                         projection,
                                                                                     scene_camera                             camera,
                                                                                     const _ogl_scene_renderer_render_mode&   render_mode,
                                                                                     bool                                     apply_shadow_mapping,
                                                                                     _ogl_scene_renderer_helper_visualization helper_visualization,
                                                                                     system_time                              frame_time);

/** TODO. **/
PUBLIC EMERALD_API void ogl_scene_renderer_release(ogl_scene_renderer renderer);

/** TODO */
PUBLIC EMERALD_API void ogl_scene_renderer_set_property(ogl_scene_renderer          renderer,
                                                        ogl_scene_renderer_property property,
                                                        const void*                 data);

/** TODO.
 *
 *  Internal usage only (exposed specifically for ogl_shadow_mapping which
 *  handles the RENDER_MODE_SHADOW_MAP render mode).
 *
 **/
PUBLIC void ogl_scene_renderer_update_current_model_matrix(system_matrix4x4 transformation_matrix,
                                                           void*            renderer);

/** TODO.
 *
 *  Internal usage only (exposed specifically for ogl_shadow_mapping which
 *  handles the RENDER_MODE_SHADOW_MAP render mode).
 *
 **/
PUBLIC void ogl_scene_renderer_update_light_properties(scene_light light,
                                                       void*       renderer);

#endif /* OGL_SCENE_RENDERER_H */