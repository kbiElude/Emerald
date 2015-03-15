/**
 *
 * Test demo application @2015
 *
 */
#include "shared.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_scene_renderer.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_multiloader.h"
#include "system/system_log.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_unpacker.h"
#include "system/system_file_multiunpacker.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"

/* A descriptor for a single scene */
typedef struct _scene_descriptor
{
    system_timeline_time duration;
    ogl_scene_renderer   renderer;
    scene                this_scene;

    _scene_descriptor()
    {
        duration   = 0;
        renderer   = NULL;
        this_scene = NULL;
    }

    ~_scene_descriptor()
    {
        ASSERT_DEBUG_SYNC(renderer == NULL,
                          "ogl_scene_renderer not deinitialized but _scene_descriptor instance about to be deallocated");

        ASSERT_DEBUG_SYNC(this_scene == NULL,
                          "scene not deinitialized but _scene_descriptor instance about to be deallocated");
    }
} _scene_descriptor;


/* Array of scene filenames to use for the demo */
system_hashed_ansi_string _scene_filenames[] =
{
    system_hashed_ansi_string_create("blob/scene1/test.packed"),
    //system_hashed_ansi_string_create("blob/scene2/test.packed"),
    //system_hashed_ansi_string_create("blob/scene3/test.packed"),
    //system_hashed_ansi_string_create("blob/scene4/test.packed")
};
const unsigned int _n_scene_filenames = sizeof(_scene_filenames) /
                                        sizeof(_scene_filenames[0]);


/* Other stuff */
ogl_texture_internalformat                  _color_shadow_map_internalformat            = OGL_TEXTURE_INTERNALFORMAT_GL_RG32F;
ogl_texture_internalformat                  _depth_shadow_map_internalformat            = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT16;
ogl_pipeline                                _pipeline                                   = NULL;
uint32_t                                    _pipeline_stage_id                          = -1;
system_timeline_time                        _playback_start_time                        = 0;
_scene_descriptor                           _scenes                [_n_scene_filenames];
system_timeline_time                        _scenes_duration_summed[_n_scene_filenames] = {0};
scene_light_shadow_map_pointlight_algorithm _shadow_map_pl_algo                         = SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID;
unsigned int                                _shadow_map_size                            = 1024;


/** Please see header for spec */
PUBLIC void state_deinit()
{
    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames;
                    ++n_scene)
    {
        if (_scenes[n_scene].renderer != NULL)
        {
            ogl_scene_renderer_release(_scenes[n_scene].renderer);

            _scenes[n_scene].renderer = NULL;
        }

        if (_scenes[n_scene].this_scene != NULL)
        {
            scene_release(_scenes[n_scene].this_scene);

            _scenes[n_scene].this_scene = NULL;
        }
    } /* for (all scenes) */

    if (_pipeline != NULL)
    {
        ogl_pipeline_release(_pipeline);

        _pipeline = NULL;
    }
}

/** Please see header for spec */
PUBLIC void state_get_current_frame_properties(__out __notnull scene*                out_current_scene,
                                               __out __notnull scene_camera*         out_current_scene_camera,
                                               __out __notnull ogl_scene_renderer*   out_current_renderer,
                                               __out           system_timeline_time* out_current_frame_time)
{
    system_timeline_time current_time        = 0;
    unsigned int         n_times_scene_shown = 0;
    static bool          is_first_call       = true;

    if (is_first_call)
    {
        _playback_start_time = system_time_now();
        is_first_call        = false;
    }
    else
    {
        current_time = system_time_now() - _playback_start_time;
    }

    n_times_scene_shown = current_time / _scenes_duration_summed[_n_scene_filenames - 1];
    current_time        = current_time % _scenes_duration_summed[_n_scene_filenames - 1];

    /* Which scene should be playing right now? */
    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames;
                    ++n_scene)
    {
        if (_scenes_duration_summed[n_scene] > current_time)
        {
            system_timeline_time prev_scene_summed_time = 0;

            if (n_scene > 0)
            {
                prev_scene_summed_time = _scenes_duration_summed[n_scene - 1];
            }

            *out_current_frame_time = current_time - prev_scene_summed_time;
            *out_current_renderer   = _scenes[n_scene].renderer;
            *out_current_scene      = _scenes[n_scene].this_scene;

            /* Also determine which camera we should be using */
            unsigned int n_current_camera = 0;
            unsigned int n_scene_cameras  = 0;

            scene_get_property(*out_current_scene,
                               SCENE_PROPERTY_N_CAMERAS,
                              &n_scene_cameras);

            n_current_camera          = n_times_scene_shown % n_scene_cameras;
            *out_current_scene_camera = scene_get_camera_by_index(*out_current_scene,
                                                                   n_current_camera);

            break;
        }
    }
}

/** Please see header for spec */
PUBLIC ogl_pipeline state_get_pipeline()
{
    return _pipeline;
}

/** Please see header for spec */
PUBLIC uint32_t state_get_pipeline_stage_id()
{
    return _pipeline_stage_id;
}

/** Please see header for spec */
PUBLIC ogl_texture_internalformat state_get_color_shadow_map_internalformat()
{
    return _color_shadow_map_internalformat;
}

/** Please see header for spec */
PUBLIC ogl_texture_internalformat state_get_depth_shadow_map_internalformat()
{
    return _depth_shadow_map_internalformat;
}

/** Please see header for spec */
PUBLIC scene_light_shadow_map_pointlight_algorithm state_get_shadow_map_pointlight_algorithm()
{
    return _shadow_map_pl_algo;
}

/** Please see header for spec */
PUBLIC uint32_t state_get_shadow_map_size()
{
    return _shadow_map_size;
}

/** Please see header for spec */
PUBLIC void state_init()
{
    const float camera_start_position[3] = {0, 0, 0};

    /* Initialize the unpackers */
    system_timeline_time      loading_time_start                    = system_time_now();
    system_file_multiunpacker multi_unpacker                        = NULL;
    system_file_serializer    scene_serializers[_n_scene_filenames] = {NULL};

    multi_unpacker = system_file_multiunpacker_create(_scene_filenames,
                                                      _n_scene_filenames);

    system_file_multiunpacker_wait_till_ready(multi_unpacker);

    /* Retrieve serializers for the scene files */
    for (unsigned int n_packed_file = 0;
                      n_packed_file < _n_scene_filenames;
                    ++n_packed_file)
    {
        /* Locate the scene file */
        bool                 scene_file_found = false;
        unsigned int         scene_file_index = -1;
        system_file_unpacker unpacker         = NULL;

        system_file_multiunpacker_get_indexed_property(multi_unpacker,
                                                       n_packed_file,
                                                       SYSTEM_FILE_MULTIUNPACKER_PROPERTY_FILE_UNPACKER,
                                                      &unpacker);

        scene_file_found = system_file_enumerator_is_file_present_in_system_file_unpacker(unpacker,
                                                                                          system_hashed_ansi_string_create("test.scene"),
                                                                                          false, /* use_exact_match */
                                                                                         &scene_file_index);

        ASSERT_DEBUG_SYNC(scene_file_found,
                          "Scene file not found in a packed file");

        system_file_unpacker_get_file_property(unpacker,
                                               scene_file_index,
                                               SYSTEM_FILE_UNPACKER_FILE_PROPERTY_FILE_SERIALIZER,
                                               scene_serializers + n_packed_file);
    } /* for (all packed files) */

    /* Load the scenes */
    scene_multiloader    loader                = scene_multiloader_create_from_system_file_serializers(_context,
                                                                                                       _n_scene_filenames,
                                                                                                       scene_serializers);
    system_timeline_time loading_time_end      = 0;
    uint32_t             loading_time_msec     = 0;
    system_timeline_time scene_duration_summed = 0;

    scene_multiloader_load_async         (loader);
    scene_multiloader_wait_until_finished(loader);

    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames;
                    ++n_scene)
    {
        scene_multiloader_get_loaded_scene(loader,
                                           n_scene,
                                          &_scenes[n_scene].this_scene);
        scene_retain                      (_scenes[n_scene].this_scene);

        /* Determine animation duration */
        float                animation_duration_float = 0.0f;
        system_timeline_time animation_duration       = 0;

        scene_get_property(_scenes[n_scene].this_scene,
                           SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                          &animation_duration_float);

        _scenes[n_scene].duration = system_time_get_timeline_time_for_msec( uint32_t(animation_duration_float * 1000.0f) );

        /* Update the summed duration array */
        _scenes_duration_summed[n_scene] = scene_duration_summed + _scenes[n_scene].duration;
        scene_duration_summed            = _scenes_duration_summed[n_scene];

        /* Initialize scene renderers */
        _scenes[n_scene].renderer = ogl_scene_renderer_create(_context,
                                                              _scenes[n_scene].this_scene);

        /* Initialize ogl_uber instances */
        ogl_scene_renderer_bake_gpu_assets(_scenes[n_scene].renderer);
    }

    scene_multiloader_release(loader);
    loader = NULL;

    system_file_multiunpacker_release(multi_unpacker);
    multi_unpacker = NULL;

    /* How much time has the loading taken? */
    loading_time_end = system_time_now();

    system_time_get_msec_for_timeline_time(loading_time_end - loading_time_start,
                                          &loading_time_msec);

    LOG_INFO("Scene loading time: %d.%d s",
             loading_time_msec / 1000,
             loading_time_msec % 1000);

    /* Update the shadow map size in all loaded scenes */
    state_set_shadow_map_size(_shadow_map_size);

    /* Retrieve shadow map internalformat & point light algorithm currently used by 
     * all lights.
     */
    bool has_updated_color_sm_internalformat = false;
    bool has_updated_depth_sm_internalformat = false;
    bool has_updated_sm_pl_algo              = false;

    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames && (!has_updated_color_sm_internalformat ||
                                                       !has_updated_depth_sm_internalformat ||
                                                       !has_updated_sm_pl_algo);
                    ++n_scene)
    {
        uint32_t n_lights = 0;

        scene_get_property(_scenes[n_scene].this_scene,
                           SCENE_PROPERTY_N_LIGHTS,
                          &n_lights);

        /* Iterate over all lights until we retrieve the property we need */
        for (unsigned int n_light = 0;
                          n_light < n_lights && (!has_updated_color_sm_internalformat ||
                                                 !has_updated_depth_sm_internalformat ||
                                                 !has_updated_sm_pl_algo);
                        ++n_light)
        {
            scene_light      current_light = scene_get_light_by_index(_scenes[n_scene].this_scene,
                                                                      n_light);
            scene_light_type current_light_type;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_TYPE,
                                    &current_light_type);

            /* color SM internalformat */
            if (!has_updated_color_sm_internalformat && current_light_type != SCENE_LIGHT_TYPE_AMBIENT)
            {
                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT_COLOR,
                                        &_color_shadow_map_internalformat);

                has_updated_color_sm_internalformat = true;
            }

            /* depth SM internalformat */
            if (!has_updated_depth_sm_internalformat && current_light_type != SCENE_LIGHT_TYPE_AMBIENT)
            {
                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT_DEPTH,
                                        &_depth_shadow_map_internalformat);

                has_updated_depth_sm_internalformat = true;
            }

            /* PL algo */
            if (!has_updated_sm_pl_algo && current_light_type == SCENE_LIGHT_TYPE_POINT)
            {
                scene_light_get_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                        &_shadow_map_pl_algo);

                has_updated_sm_pl_algo = true;
            } /* if (!has_updated_sm_pl_algo) */
        } /* for (all scene lights) */
    } /* for (all scenes) */

    /* Construct the pipeline object */
    _pipeline = ogl_pipeline_create(_context,
                                    true, /* should_overlay_performance_info */
                                    system_hashed_ansi_string_create("Pipeline") );

    _pipeline_stage_id = ogl_pipeline_add_stage(_pipeline);

    ogl_pipeline_add_stage_step(_pipeline,
                                _pipeline_stage_id,
                                system_hashed_ansi_string_create("Scene rendering"),
                                _render_scene,
                                NULL);
}

/** Please see header for spec */
PUBLIC void state_set_color_shadow_map_internalformat(__in ogl_texture_internalformat new_internalformat)
{
    /* Wait for the current frame to render and lock the rendering pipeline, while
     * we adjust the shadow map size..
     */
    ogl_rendering_handler_lock_bound_context(_rendering_handler);

    /* Update shadow map size for all scene lights */
    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames;
                    ++n_scene)
    {
        scene        current_scene = _scenes[n_scene].this_scene;
        unsigned int n_lights      = 0;

        scene_get_property(current_scene,
                           SCENE_PROPERTY_N_LIGHTS,
                          &n_lights);

        for (unsigned int n_light = 0;
                          n_light < n_lights;
                        ++n_light)
        {
            scene_light      current_light = scene_get_light_by_index(current_scene,
                                                                      n_light);
            scene_light_type current_light_type;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_TYPE,
                                    &current_light_type);

            if (current_light_type != SCENE_LIGHT_TYPE_AMBIENT)
            {
                scene_light_set_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT_COLOR,
                                        &new_internalformat);
            } /* if (current light is not an ambient light) */
        } /* for (all scene lights) */
    } /* for (all loaded scenes) */

    _depth_shadow_map_internalformat = new_internalformat;

    /* Unlock the rendering process */
    ogl_rendering_handler_unlock_bound_context(_rendering_handler);
}

/** Please see header for spec */
PUBLIC void state_set_depth_shadow_map_internalformat(__in ogl_texture_internalformat new_internalformat)
{
    /* Wait for the current frame to render and lock the rendering pipeline, while
     * we adjust the shadow map size..
     */
    ogl_rendering_handler_lock_bound_context(_rendering_handler);

    /* Update shadow map size for all scene lights */
    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames;
                    ++n_scene)
    {
        scene        current_scene = _scenes[n_scene].this_scene;
        unsigned int n_lights      = 0;

        scene_get_property(current_scene,
                           SCENE_PROPERTY_N_LIGHTS,
                          &n_lights);

        for (unsigned int n_light = 0;
                          n_light < n_lights;
                        ++n_light)
        {
            scene_light      current_light = scene_get_light_by_index(current_scene,
                                                                      n_light);
            scene_light_type current_light_type;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_TYPE,
                                    &current_light_type);

            if (current_light_type != SCENE_LIGHT_TYPE_AMBIENT)
            {
                scene_light_set_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT_DEPTH,
                                        &new_internalformat);
            } /* if (current light is not an ambient light) */
        } /* for (all scene lights) */
    } /* for (all loaded scenes) */

    _depth_shadow_map_internalformat = new_internalformat;

    /* Unlock the rendering process */
    ogl_rendering_handler_unlock_bound_context(_rendering_handler);
}

/** Please see header for spec */
PUBLIC void state_set_shadow_map_pointlight_algorithm(__in scene_light_shadow_map_pointlight_algorithm pl_algo)
{
    /* Wait for the current frame to render and lock the rendering pipeline, while
     * we adjust the shadow map size..
     */
    ogl_rendering_handler_lock_bound_context(_rendering_handler);

    /* Update PL algorithm for all point lights in all scenes we know of */
    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames;
                    ++n_scene)
    {
        scene        current_scene = _scenes[n_scene].this_scene;
        unsigned int n_lights      = 0;

        scene_get_property(current_scene,
                           SCENE_PROPERTY_N_LIGHTS,
                          &n_lights);

        for (unsigned int n_light = 0;
                          n_light < n_lights;
                        ++n_light)
        {
            scene_light      current_light = scene_get_light_by_index(current_scene,
                                                                      n_light);
            scene_light_type current_light_type;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_TYPE,
                                    &current_light_type);

            if (current_light_type == SCENE_LIGHT_TYPE_POINT)
            {
                scene_light_set_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,
                                        &pl_algo);
            } /* if (current light is not an ambient light) */
        } /* for (all scene lights) */
    } /* for (all loaded scenes) */

    _shadow_map_pl_algo = pl_algo;

    /* Unlock the rendering process */
    ogl_rendering_handler_unlock_bound_context(_rendering_handler);
}

/** Please see header for spec */
PUBLIC void state_set_shadow_map_size(__in unsigned int new_shadow_map_size)
{
    /* Wait for the current frame to render and lock the rendering pipeline, while
     * we adjust the shadow map size..
     */
    ogl_rendering_handler_lock_bound_context(_rendering_handler);

    /* Update shadow map size for all scene lights */
    for (unsigned int n_scene = 0;
                      n_scene < _n_scene_filenames;
                    ++n_scene)
    {
        scene        current_scene = _scenes[n_scene].this_scene;
        unsigned int n_lights      = 0;

        scene_get_property(current_scene,
                           SCENE_PROPERTY_N_LIGHTS,
                          &n_lights);

        for (unsigned int n_light = 0;
                          n_light < n_lights;
                        ++n_light)
        {
            scene_light      current_light = scene_get_light_by_index(current_scene,
                                                                      n_light);
            scene_light_type current_light_type;

            scene_light_get_property(current_light,
                                     SCENE_LIGHT_PROPERTY_TYPE,
                                    &current_light_type);

            if (current_light_type != SCENE_LIGHT_TYPE_AMBIENT)
            {
                unsigned int final_shadow_map_size[2] =
                {
                    new_shadow_map_size,
                    new_shadow_map_size
                };

                scene_light_set_property(current_light,
                                         SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,
                                         final_shadow_map_size);
            } /* if (current light is not an ambient light) */
        } /* for (all scene lights) */
    } /* for (all loaded scenes) */

    _shadow_map_size = new_shadow_map_size;

    /* Unlock the rendering process */
    ogl_rendering_handler_unlock_bound_context(_rendering_handler);
}
