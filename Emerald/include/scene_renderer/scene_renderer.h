/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef SCENE_RENDERER_H
#define SCENE_RENDERER_H

#include "mesh/mesh_types.h"
#include "ral/ral_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"

#ifdef __linux__
    #include <cfloat>
#endif


#define DEFAULT_AABB_MAX_VALUE (FLT_MIN)
#define DEFAULT_AABB_MIN_VALUE (FLT_MAX)


typedef enum
{
    /* general property; ral_context */
    SCENE_RENDERER_PROPERTY_CONTEXT_RAL,

    /* general property; ral_texture_view */
    SCENE_RENDERER_PROPERTY_FORWARD_COLOR_RT,

    /* general property; ral_texture_view */
    SCENE_RENDERER_PROPERTY_FORWARD_DEPTH_RT,

    /* general property; scene_graph */
    SCENE_RENDERER_PROPERTY_GRAPH,

    /* indexed property; key: mesh id // value: mesh */
    SCENE_RENDERER_PROPERTY_MESH_INSTANCE,

    /* indexed property; key: mesh id // value: system_matrix4x4.
     *
     * May not return any value if the model was frustum-culled out.
     */
    SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX,

    /* indexed property; key: mesh id // value: system_matrix4x4 */
    SCENE_RENDERER_PROPERTY_MESH_NORMAL_MATRIX,

    /* general property; scene_renderer_sm */
    SCENE_RENDERER_PROPERTY_SHADOW_MAPPING_MANAGER,

    /* general property; float[3] */
    SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX,

    /* general property; float[3] */
    SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN,

    /* general property; system_matrix4x4 */
    SCENE_RENDERER_PROPERTY_VP,
} scene_renderer_property;


/** TODO.
 *
 *  Bakes in advance all GPU assets required to render a given scene.
 *
 */
PUBLIC void scene_renderer_bake_gpu_assets(scene_renderer renderer);

/** TODO.
 *
 *  SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES behavior implementation
 *  is based on http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes-ii/
 *  and http://gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf.
 *
 *  This function also adjusts scene_renderer's max & min AABB coordinates.
 *
 *  @param renderer      TODO
 *  @param mesh_gpu      TODO
 *  @param behavior      TODO
 *  @param behavior_data Additional behavior-specific argument. Please see documentation of
 *                       _scene_renderer_frustum_culling_behavior for more details.
 *
 *  @return true if the mesh is inside OR intersects the frustum, false otherwise.
 *
 *  Note: if "true" is returned, SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MAX and
 *        SCENE_RENDERER_PROPERTY_VISIBLE_WORLD_AABB_MIN properties of @param renderer
 *        are also adjusted.
 */
PUBLIC bool scene_renderer_cull_against_frustum(scene_renderer                          renderer,
                                                mesh                                    mesh_gpu,
                                                scene_renderer_frustum_culling_behavior behavior,
                                                const void*                             behavior_data);

/** TODO. **/
PUBLIC EMERALD_API scene_renderer scene_renderer_create(ral_context context,
                                                        scene       scene);

/** TODO. Internal usage only */
PUBLIC void scene_renderer_get_indexed_property(const scene_renderer    renderer,
                                                scene_renderer_property property,
                                                uint32_t                index,
                                                void*                   out_result_ptr);

/** TODO.
 *
 *  TODO: state-ify render_mode / shadow_mapping_type / helper_visualization arguments..
 *        (issue #126)
 *
 *  @param renderer             TODO
 *  @param view                 View matrix, to be used for rendering meshes in the final pass.
 *  @param projection           Projection matrix, to be used for rendering the scene meshes in the
 *                              final pass.
 *  @param camera               scene_camera instance which describes properties of the camera which
 *                              should be used for rendering.
 *                              TODO: There's a bug currently which causes the module to use camera's
 *                                    transformation matrix for shadow mapping, and view+projection
 *                                    matrices for final pass rendering. (issue #126)
 *  @param render_mode          TODO
 *  @param apply_shadow_mapping TODO
 *  @param helper_visualization TODO
 *  @param frame_time           TODO
 *
 **/
PUBLIC EMERALD_API ral_present_task scene_renderer_get_present_task_for_scene_graph(scene_renderer                      renderer,
                                                                                    system_matrix4x4                    view,
                                                                                    system_matrix4x4                    projection,
                                                                                    scene_camera                        camera,
                                                                                    const scene_renderer_render_mode&   render_mode,
                                                                                    bool                                apply_shadow_mapping,
                                                                                    scene_renderer_helper_visualization helper_visualization,
                                                                                    system_time                         frame_time,
                                                                                    ral_texture_view                    color_rt,
                                                                                    ral_texture_view                    depth_rt);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_get_property(const scene_renderer    renderer,
                                                    scene_renderer_property property,
                                                    void*                   out_result_ptr);

/** TODO. **/
PUBLIC EMERALD_API void scene_renderer_release(scene_renderer renderer);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_set_property(scene_renderer          renderer,
                                                    scene_renderer_property property,
                                                    const void*             data);

/** TODO.
 *
 *  Internal usage only (exposed specifically for scene_renderer_sm which
 *  handles the RENDER_MODE_SHADOW_MAP render mode).
 *
 **/
PUBLIC void scene_renderer_update_current_model_matrix(system_matrix4x4 transformation_matrix,
                                                       void*            renderer);

/** TODO.
 *
 *  Internal usage only (exposed specifically for scene_renderer_sm which
 *  handles the RENDER_MODE_SHADOW_MAP render mode).
 *
 **/
PUBLIC void scene_renderer_update_light_properties(scene_light light,
                                                   void*       renderer);

#endif /* SCENE_RENDERER_H */