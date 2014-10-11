/**
 *
 * Particles test app (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_uber.h"
#include "scene/scene.h"
#include "sh/sh_projector_combiner.h"
#include "sh/sh_samples.h"
#include "shaders/shaders_fragment_uber.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_variant.h"
#include "stage_step_mesh.h"
#include "../include/main.h"

ogl_uber_item_id              _uber_light_id                             = -1;
_ogl_uber_light_sh_data       _stage_step_light_sh_data                  = {0};
ogl_texture                   _stage_step_light_sh_data_tbo              = 0;
ogl_context                   _stage_step_mesh_context                   = NULL;
system_matrix4x4              _stage_step_mesh_model_matrix              = NULL;
sh_projector_combiner         _stage_step_mesh_scene_projection_combiner = NULL;
uint32_t                      _stage_step_mesh_step_id                   = -1;
ogl_uber                      _stage_step_mesh_uber                      = NULL;
system_matrix4x4              _stage_step_mesh_view_matrix               = NULL;

/** TODO */
static void _stage_step_mesh_render(ogl_context context, system_timeline_time time, void* not_used)
{
    const float*                      camera_location   = ogl_flyby_get_camera_location(_stage_step_mesh_context);
    const ogl_context_gl_entrypoints* entrypoints       = NULL;
    const float                       light_color[]     = {0.85f, 0.85f, 0.85f};
    const float                       light_direction[] = {0, 0, -1};
    system_matrix4x4                  mvp               = NULL;
    system_matrix4x4                  projection_matrix = main_get_projection_matrix();
    scene                             scene             = main_get_scene();
    mesh                              scene_mesh        = main_get_scene_mesh();

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entrypoints);

    entrypoints->pGLClearColor(0, 0.25f, 0, 0);
    entrypoints->pGLClear     (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    entrypoints->pGLEnable(GL_DEPTH_TEST);

    ogl_flyby_get_view_matrix(context, _stage_step_mesh_view_matrix);
    mvp = system_matrix4x4_create_by_mul(projection_matrix, _stage_step_mesh_view_matrix);

    ogl_uber_set_shader_general_property(_stage_step_mesh_uber,                 OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,       camera_location);
    ogl_uber_set_shader_general_property(_stage_step_mesh_uber,                 OGL_UBER_GENERAL_PROPERTY_VP,                    mvp);
    ogl_uber_set_shader_item_property   (_stage_step_mesh_uber, _uber_light_id, OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_COLOR,     light_color);
    ogl_uber_set_shader_item_property   (_stage_step_mesh_uber, _uber_light_id, OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION, light_direction);
    ogl_uber_set_shader_item_property   (_stage_step_mesh_uber, _uber_light_id, OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_SH_DATA,    &_stage_step_light_sh_data);

    ogl_uber_rendering_start(_stage_step_mesh_uber);
    {
        ogl_uber_rendering_render(_stage_step_mesh_uber, 1, &_stage_step_mesh_model_matrix, &scene_mesh, scene);
    }
    ogl_uber_rendering_stop(_stage_step_mesh_uber);

    system_matrix4x4_release(mvp);
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_mesh_deinit(__in __notnull ogl_context,
                                                          __in __notnull ogl_pipeline)
{
    if (_stage_step_mesh_context != NULL)
    {
        ogl_context_release(_stage_step_mesh_context);
    }

    if (_stage_step_mesh_uber != NULL)
    {
        ogl_uber_release(_stage_step_mesh_uber);
    }

    if (_stage_step_mesh_scene_projection_combiner != NULL)
    {
        sh_projector_combiner_release(_stage_step_mesh_scene_projection_combiner);
    }

    if (_stage_step_mesh_view_matrix != NULL)
    {
        system_matrix4x4_release(_stage_step_mesh_view_matrix);

        _stage_step_mesh_view_matrix = NULL;
    }

    ogl_texture_release(_stage_step_light_sh_data_tbo);
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_mesh_init(__in __notnull ogl_context  context,
                                                        __in __notnull ogl_pipeline pipeline,
                                                        __in           uint32_t     stage_id)
{
    _stage_step_mesh_context = context;
    ogl_context_retain(context);

    /* Initialize shading program */
    const shaders_fragment_uber_diffuse       diffuse_uber_light    = UBER_DIFFUSE_LIGHT_PROJECTION_SH3;
    const shaders_fragment_uber_specular      specular_uber_light   = UBER_SPECULAR_PHONG;

    _stage_step_mesh_uber = ogl_uber_create        (_stage_step_mesh_context, system_hashed_ansi_string_create("Uber program") );
    _uber_light_id        = ogl_uber_add_light_item(_stage_step_mesh_uber,    diffuse_uber_light, specular_uber_light, 0, NULL);

    /* Initialize samples - we need it for calculating the SH ambient projection */
    sh_samples samples = sh_samples_create(context, 100 * 100, 3 /* bands */, system_hashed_ansi_string_create("SH samples") );

    sh_samples_execute(samples);

    /* Initialize SH projection */
    sh_projection_id projection_id          = 0;
    curve_container  zenith_luminance_curve = NULL;
    system_variant   temp_variant           = system_variant_create(SYSTEM_VARIANT_FLOAT);

    _stage_step_mesh_scene_projection_combiner = sh_projector_combiner_create(context, SH_COMPONENTS_RGB, samples, system_hashed_ansi_string_create("SH projection combiner") );

    sh_projector_combiner_add_sh_projection                 (_stage_step_mesh_scene_projection_combiner, SH_PROJECTION_TYPE_CIE_OVERCAST, &projection_id);
    sh_projector_combiner_get_sh_projection_property_details(_stage_step_mesh_scene_projection_combiner, projection_id, SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE, &zenith_luminance_curve);

    system_variant_set_float         (temp_variant, .025f);
    curve_container_set_default_value(zenith_luminance_curve, 0, temp_variant);

    /* Calculate SH projection coefficients */
    sh_projector_combiner_execute(_stage_step_mesh_scene_projection_combiner, 0);

    /* Configure ambient projection SH data representation */
    memset(&_stage_step_light_sh_data, 0, sizeof(_stage_step_light_sh_data) );

    sh_projector_combiner_get_sh_projection_result_bo_data_details(_stage_step_mesh_scene_projection_combiner,
                                                                  &_stage_step_light_sh_data.bo_id,
                                                                  &_stage_step_light_sh_data.bo_offset,
                                                                   NULL,
                                                                  &_stage_step_light_sh_data_tbo);

    /* Good to release SH samples at this point */
    sh_samples_release    (samples);
    system_variant_release(temp_variant);

    /* Initialize matrices */
    _stage_step_mesh_model_matrix = system_matrix4x4_create();
    _stage_step_mesh_view_matrix  = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(_stage_step_mesh_model_matrix);

    /* Add the step */
    ogl_pipeline_add_stage_step(pipeline, stage_id, system_hashed_ansi_string_create("Mesh rendering"), _stage_step_mesh_render,  NULL);
}

/** Please see header for specification */
PUBLIC uint32_t stage_particle_get_stage_step_id()
{
    return _stage_step_mesh_step_id;
}
