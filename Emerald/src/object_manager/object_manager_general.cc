/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "object_manager/object_manager_directory.h"
#include "object_manager/object_manager_general.h"
#include "object_manager/object_manager_item.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/* Private variables */
object_manager_directory _root = NULL;

static system_hashed_ansi_string _object_type_audio_stream_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_collada_data_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_context_menu_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_curve_container_hashed_ansi_string                       = NULL;
static system_hashed_ansi_string _object_type_demo_flyby_hashed_ansi_string                            = NULL;
static system_hashed_ansi_string _object_type_demo_timeline_hashed_ansi_string                         = NULL;
static system_hashed_ansi_string _object_type_demo_timeline_segment_hashed_ansi_string                 = NULL;
static system_hashed_ansi_string _object_type_gfx_bfg_font_table_hashed_ansi_string                    = NULL;
static system_hashed_ansi_string _object_type_gfx_image_hashed_ansi_string                             = NULL;
static system_hashed_ansi_string _object_type_glsl_shader_constructor_hashed_ansi_string               = NULL;
static system_hashed_ansi_string _object_type_mesh_hashed_ansi_string                                  = NULL;
static system_hashed_ansi_string _object_type_mesh_marchingcubes_hashed_ansi_string                    = NULL;
static system_hashed_ansi_string _object_type_mesh_material_hashed_ansi_string                         = NULL;
static system_hashed_ansi_string _object_type_ocl_context_hashed_ansi_string                           = NULL;
static system_hashed_ansi_string _object_type_ocl_kdtree_hashed_ansi_string                            = NULL;
static system_hashed_ansi_string _object_type_ocl_kernel_hashed_ansi_string                            = NULL;
static system_hashed_ansi_string _object_type_ocl_program_hashed_ansi_string                           = NULL;
static system_hashed_ansi_string _object_type_ogl_context_hashed_ansi_string                           = NULL;
static system_hashed_ansi_string _object_type_ogl_pipeline_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_ogl_rendering_handler_hashed_ansi_string                 = NULL;
static system_hashed_ansi_string _object_type_ogl_textures_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_postprocessing_bloom_hashed_ansi_string                  = NULL;
static system_hashed_ansi_string _object_type_postprocessing_blur_poisson_hashed_ansi_string           = NULL;
static system_hashed_ansi_string _object_type_postprocessing_motion_blur_hashed_ansi_string            = NULL;
static system_hashed_ansi_string _object_type_postprocessing_reinhard_tonemap_hashed_ansi_string       = NULL;
static system_hashed_ansi_string _object_type_procedural_mesh_box_hashed_ansi_string                   = NULL;
static system_hashed_ansi_string _object_type_procedural_mesh_circle_hashed_ansi_string                = NULL;
static system_hashed_ansi_string _object_type_procedural_mesh_sphere_hashed_ansi_string                = NULL;
static system_hashed_ansi_string _object_type_procedural_uv_generator_hashed_ansi_string               = NULL;
static system_hashed_ansi_string _object_type_programs_curve_editor_curvebackground_hashed_ansi_string = NULL;
static system_hashed_ansi_string _object_type_programs_curve_editor_lerp_hashed_ansi_string            = NULL;
static system_hashed_ansi_string _object_type_programs_curve_editor_quadselector_hashed_ansi_string    = NULL;
static system_hashed_ansi_string _object_type_programs_curve_editor_static_hashed_ansi_string          = NULL;
static system_hashed_ansi_string _object_type_programs_curve_editor_tcb_hashed_ansi_string             = NULL;
static system_hashed_ansi_string _object_type_ragl_buffers_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_ragl_sync_hashed_ansi_string                             = NULL;
static system_hashed_ansi_string _object_type_ragl_textures_hashed_ansi_string                         = NULL;
static system_hashed_ansi_string _object_type_ral_context_hashed_ansi_string                           = NULL;
static system_hashed_ansi_string _object_type_scalar_field_metaballs_hashed_ansi_string                = NULL;
static system_hashed_ansi_string _object_type_scene_hashed_ansi_string                                 = NULL;
static system_hashed_ansi_string _object_type_scene_camera_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_scene_curve_hashed_ansi_string                           = NULL;
static system_hashed_ansi_string _object_type_scene_light_hashed_ansi_string                           = NULL;
static system_hashed_ansi_string _object_type_scene_material_hashed_ansi_string                        = NULL;
static system_hashed_ansi_string _object_type_scene_mesh_hashed_ansi_string                            = NULL;
static system_hashed_ansi_string _object_type_scene_renderer_uber_hashed_ansi_string                   = NULL;
static system_hashed_ansi_string _object_type_scene_surface_hashed_ansi_string                         = NULL;
static system_hashed_ansi_string _object_type_scene_texture_hashed_ansi_string                         = NULL;
static system_hashed_ansi_string _object_type_sh_projector_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_sh_projector_combiner_hashed_ansi_string                 = NULL;
static system_hashed_ansi_string _object_type_sh_rot_hashed_ansi_string                                = NULL;
static system_hashed_ansi_string _object_type_sh_samples_hashed_ansi_string                            = NULL;
static system_hashed_ansi_string _object_type_sh_shaders_object_hashed_ansi_string                     = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_rgb_to_Yxy                              = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_static                                  = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_texture2D_filmic                        = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_texture2D_filmic_customizable           = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_texture2D_linear                        = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_texture2D_plain                         = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_texture2D_reinhardt                     = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_uber                                    = NULL;
static system_hashed_ansi_string _object_type_shaders_fragment_Yxy_to_rgb                              = NULL;
static system_hashed_ansi_string _object_type_shaders_vertex_combinedmvp_generic                       = NULL;
static system_hashed_ansi_string _object_type_shaders_vertex_combinedmvp_simplified_two_point          = NULL;
static system_hashed_ansi_string _object_type_shaders_vertex_fullscreen                                = NULL;
static system_hashed_ansi_string _object_type_shaders_vertex_uber                                      = NULL;
static system_hashed_ansi_string _object_type_system_file_serializer_hashed_ansi_string                = NULL;
static system_hashed_ansi_string _object_type_system_randomizer_hashed_ansi_string                     = NULL;
static system_hashed_ansi_string _object_type_system_window_hashed_ansi_string                         = NULL;
static system_hashed_ansi_string _object_type_ui_hashed_ansi_string                                    = NULL;
static system_hashed_ansi_string _object_type_unknown_hashed_ansi_string                               = NULL;
static system_hashed_ansi_string _object_type_varia_curve_renderer_hashed_ansi_string                  = NULL;
static system_hashed_ansi_string _object_type_varia_primitive_renderer_hashed_ansi_string              = NULL;
static system_hashed_ansi_string _object_type_varia_skybox_hashed_ansi_string                          = NULL;
static system_hashed_ansi_string _object_type_varia_text_renderer_hashed_ansi_string                   = NULL;


/** Please see header for specification */
PUBLIC system_hashed_ansi_string object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_manager_object_type object_type)
{
    switch (object_type)
    {
        case OBJECT_TYPE_AUDIO_STREAM:                                    return _object_type_audio_stream_hashed_ansi_string;
        case OBJECT_TYPE_COLLADA_DATA:                                    return _object_type_collada_data_hashed_ansi_string;
        case OBJECT_TYPE_CONTEXT_MENU:                                    return _object_type_context_menu_hashed_ansi_string;
        case OBJECT_TYPE_CURVE_CONTAINER:                                 return _object_type_curve_container_hashed_ansi_string;
        case OBJECT_TYPE_DEMO_FLYBY:                                      return _object_type_demo_flyby_hashed_ansi_string;
        case OBJECT_TYPE_DEMO_TIMELINE:                                   return _object_type_demo_timeline_hashed_ansi_string;
        case OBJECT_TYPE_DEMO_TIMELINE_SEGMENT:                           return _object_type_demo_timeline_segment_hashed_ansi_string;
        case OBJECT_TYPE_GFX_BFG_FONT_TABLE:                              return _object_type_gfx_bfg_font_table_hashed_ansi_string;
        case OBJECT_TYPE_GFX_IMAGE:                                       return _object_type_gfx_image_hashed_ansi_string;
        case OBJECT_TYPE_GLSL_SHADER_CONSTRUCTOR:                         return _object_type_glsl_shader_constructor_hashed_ansi_string;
        case OBJECT_TYPE_MESH:                                            return _object_type_mesh_hashed_ansi_string;
        case OBJECT_TYPE_MESH_MARCHINGCUBES:                              return _object_type_mesh_marchingcubes_hashed_ansi_string;
        case OBJECT_TYPE_MESH_MATERIAL:                                   return _object_type_mesh_material_hashed_ansi_string;
        case OBJECT_TYPE_OCL_CONTEXT:                                     return _object_type_ocl_context_hashed_ansi_string;
        case OBJECT_TYPE_OCL_KDTREE:                                      return _object_type_ocl_kdtree_hashed_ansi_string;
        case OBJECT_TYPE_OCL_KERNEL:                                      return _object_type_ocl_kernel_hashed_ansi_string;
        case OBJECT_TYPE_OCL_PROGRAM:                                     return _object_type_ocl_program_hashed_ansi_string;
        case OBJECT_TYPE_OGL_CONTEXT:                                     return _object_type_ogl_context_hashed_ansi_string;
        case OBJECT_TYPE_OGL_PIPELINE:                                    return _object_type_ogl_pipeline_hashed_ansi_string;
        case OBJECT_TYPE_OGL_RENDERING_HANDLER:                           return _object_type_ogl_rendering_handler_hashed_ansi_string;
        case OBJECT_TYPE_OGL_TEXTURE:                                     return _object_type_ogl_textures_hashed_ansi_string;
        case OBJECT_TYPE_POSTPROCESSING_BLOOM:                            return _object_type_postprocessing_bloom_hashed_ansi_string;
        case OBJECT_TYPE_POSTPROCESSING_BLUR_POISSON:                     return _object_type_postprocessing_blur_poisson_hashed_ansi_string;
        case OBJECT_TYPE_POSTPROCESSING_MOTION_BLUR:                      return _object_type_postprocessing_motion_blur_hashed_ansi_string;
        case OBJECT_TYPE_POSTPROCESSING_REINHARD_TONEMAP:                 return _object_type_postprocessing_reinhard_tonemap_hashed_ansi_string;
        case OBJECT_TYPE_PROCEDURAL_MESH_BOX:                             return _object_type_procedural_mesh_box_hashed_ansi_string;
        case OBJECT_TYPE_PROCEDURAL_MESH_CIRCLE:                          return _object_type_procedural_mesh_circle_hashed_ansi_string;
        case OBJECT_TYPE_PROCEDURAL_MESH_SPHERE:                          return _object_type_procedural_mesh_sphere_hashed_ansi_string;
        case OBJECT_TYPE_PROCEDURAL_UV_GENERATOR:                         return _object_type_procedural_uv_generator_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_CURVEBACKGROUND:           return _object_type_programs_curve_editor_curvebackground_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_LERP:                      return _object_type_programs_curve_editor_lerp_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_QUADSELECTOR:              return _object_type_programs_curve_editor_quadselector_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_STATIC:                    return _object_type_programs_curve_editor_static_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_TCB:                       return _object_type_programs_curve_editor_tcb_hashed_ansi_string;
        case OBJECT_TYPE_RAGL_BUFFERS:                                    return _object_type_ragl_buffers_hashed_ansi_string;
        case OBJECT_TYPE_RAGL_SYNC:                                       return _object_type_ragl_sync_hashed_ansi_string;
        case OBJECT_TYPE_RAGL_TEXTURES:                                   return _object_type_ragl_textures_hashed_ansi_string;
        case OBJECT_TYPE_RAL_CONTEXT:                                     return _object_type_ral_context_hashed_ansi_string;
        case OBJECT_TYPE_SCALAR_FIELD_METABALLS:                          return _object_type_scalar_field_metaballs_hashed_ansi_string;
        case OBJECT_TYPE_SCENE:                                           return _object_type_scene_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_CAMERA:                                    return _object_type_scene_camera_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_CURVE:                                     return _object_type_scene_curve_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_LIGHT:                                     return _object_type_scene_light_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_MATERIAL:                                  return _object_type_scene_material_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_MESH:                                      return _object_type_scene_mesh_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_RENDERER_UBER:                             return _object_type_scene_renderer_uber_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_SURFACE:                                   return _object_type_scene_surface_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_TEXTURE:                                   return _object_type_scene_texture_hashed_ansi_string;
        case OBJECT_TYPE_SH_PROJECTOR:                                    return _object_type_sh_projector_hashed_ansi_string;
        case OBJECT_TYPE_SH_PROJECTOR_COMBINER:                           return _object_type_sh_projector_combiner_hashed_ansi_string;
        case OBJECT_TYPE_SH_ROT:                                          return _object_type_sh_rot_hashed_ansi_string;
        case OBJECT_TYPE_SH_SAMPLES:                                      return _object_type_sh_samples_hashed_ansi_string;
        case OBJECT_TYPE_SH_SHADERS_OBJECT:                               return _object_type_sh_shaders_object_hashed_ansi_string;
        case OBJECT_TYPE_SHADERS_FRAGMENT_RGB_TO_YXY:                     return _object_type_shaders_fragment_rgb_to_Yxy;
        case OBJECT_TYPE_SHADERS_FRAGMENT_STATIC:                         return _object_type_shaders_fragment_static;
        case OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_FILMIC:               return _object_type_shaders_fragment_texture2D_filmic;
        case OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_FILMIC_CUSTOMIZABLE:  return _object_type_shaders_fragment_texture2D_filmic_customizable;
        case OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_LINEAR:               return _object_type_shaders_fragment_texture2D_linear;
        case OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_PLAIN:                return _object_type_shaders_fragment_texture2D_plain;
        case OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_REINHARDT:            return _object_type_shaders_fragment_texture2D_reinhardt;
        case OBJECT_TYPE_SHADERS_FRAGMENT_UBER:                           return _object_type_shaders_fragment_uber;
        case OBJECT_TYPE_SHADERS_FRAGMENT_YXY_TO_RGB:                     return _object_type_shaders_fragment_Yxy_to_rgb;
        case OBJECT_TYPE_SHADERS_VERTEX_FULLSCREEN:                       return _object_type_shaders_vertex_fullscreen;
        case OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_GENERIC:              return _object_type_shaders_vertex_combinedmvp_generic;
        case OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_SIMPLIFIED_TWO_POINT: return _object_type_shaders_vertex_combinedmvp_simplified_two_point;
        case OBJECT_TYPE_SHADERS_VERTEX_UBER:                             return _object_type_shaders_vertex_uber;
        case OBJECT_TYPE_SYSTEM_FILE_SERIALIZER:                          return _object_type_system_file_serializer_hashed_ansi_string;
        case OBJECT_TYPE_SYSTEM_RANDOMIZER:                               return _object_type_system_randomizer_hashed_ansi_string;
        case OBJECT_TYPE_SYSTEM_WINDOW:                                   return _object_type_system_window_hashed_ansi_string;
        case OBJECT_TYPE_UI:                                              return _object_type_ui_hashed_ansi_string;
        case OBJECT_TYPE_VARIA_CURVE_RENDERER:                            return _object_type_varia_curve_renderer_hashed_ansi_string;
        case OBJECT_TYPE_VARIA_PRIMITIVE_RENDERER:                        return _object_type_varia_primitive_renderer_hashed_ansi_string;
        case OBJECT_TYPE_VARIA_SKYBOX:                                    return _object_type_varia_skybox_hashed_ansi_string;
        case OBJECT_TYPE_VARIA_TEXT_RENDERER:                             return _object_type_varia_text_renderer_hashed_ansi_string;
        default:                                                          return _object_type_unknown_hashed_ansi_string;
    }

}

/** TODO */
PRIVATE void _object_manager_delete_empty_subdirectories(object_manager_directory directory)
{
    uint32_t n_subdirectories = object_manager_directory_get_amount_of_subdirectories_for_directory(directory);

    if (n_subdirectories != 0)
    {
        system_resizable_vector subdirectories = system_resizable_vector_create(n_subdirectories);

        /* Cache subdirectory names */
        for (uint32_t n = 0;
                      n < n_subdirectories;
                    ++n)
        {
            object_manager_directory subdirectory = object_manager_directory_get_subdirectory_at(directory,
                                                                                                 n);

            ASSERT_DEBUG_SYNC(subdirectory != NULL,
                              "Subdirectory cannot be null.");

            system_resizable_vector_push(subdirectories,
                                         subdirectory);
        }

        /* Iterate through cached subdirectory names and recursively call the function */
        while (true)
        {
            object_manager_directory subdirectory = NULL;
            bool                     result       = system_resizable_vector_pop(subdirectories,
                                                                               &subdirectory);

            if (result)
            {
                _object_manager_delete_empty_subdirectories(subdirectory);

                if (object_manager_directory_get_amount_of_children_for_directory(subdirectory) == 0)
                {
                    object_manager_directory_delete_directory(directory,
                                                              object_manager_directory_get_name(subdirectory) );
                }
            }
            else
            {
                break;
            }
        }

        system_resizable_vector_release(subdirectories);
    }
}


/** Please see header for specification */
PUBLIC void _object_manager_deinit()
{
    /* If default directories are still there, remove them if they contain no elements */
    for (object_manager_object_type object_type  = OBJECT_TYPE_FIRST;
                                    object_type != OBJECT_TYPE_LAST;
                             ((int&)object_type)++)
    {
        system_hashed_ansi_string subdirectory_name = object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_type);
        object_manager_directory  subdirectory      = object_manager_directory_find_subdirectory                             (_root,
                                                                                                                              subdirectory_name);

        if (subdirectory != NULL)
        {
            _object_manager_delete_empty_subdirectories(subdirectory);

            if (object_manager_directory_get_amount_of_children_for_directory(subdirectory) == 0)
            {
                if (!object_manager_directory_delete_directory(_root,
                                                               subdirectory_name) )
                {
                    LOG_ERROR("Could not delete subdirectory [%s] while deinitializing object manager",
                              system_hashed_ansi_string_get_buffer(subdirectory_name)
                             );
                }
            }
            else
            {
                LOG_ERROR("Subdirectory [%s] continues to contain items while deinitializing object manager",
                          system_hashed_ansi_string_get_buffer(subdirectory_name) 
                         );
            }
        }
    }

    /* Make sure no objects are left! */
    object_manager_directory_set_verbose_mode(true);

    uint32_t n_elements = object_manager_directory_get_amount_of_children_for_directory(_root);

    if (n_elements != 0)
    {
        LOG_ERROR("Resource leak detected upon exiting! %u elements are still there!",
                  n_elements);
    }

    /* Now we're cool to release the storage */
    if (_root != NULL)
    {
        object_manager_directory_release(_root);

        _root = NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API object_manager_directory object_manager_get_directory(system_hashed_ansi_string path)
{
    return object_manager_directory_find_subdirectory(_root, path);
}

/** Please see header for specification */
PUBLIC system_hashed_ansi_string object_manager_get_object_path(system_hashed_ansi_string  object_name,
                                                                object_manager_object_type object_type,
                                                                system_hashed_ansi_string  scene_name)
{
    /* Come up with a path which also includes owner scene's name.
     * This helps us avoid naming collisions.
     */
    system_hashed_ansi_string object_type_has = object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_type);
    system_hashed_ansi_string result          = NULL;

    if (scene_name != NULL)
    {
        static const char* limiter        = "\\";
               const char* scene_name_raw = system_hashed_ansi_string_get_buffer(scene_name);

        const char* final_object_name_parts[] =
        {
            limiter,
            system_hashed_ansi_string_get_buffer(object_type_has),
            limiter,
            scene_name_raw,
            limiter,
            system_hashed_ansi_string_get_buffer(object_name)
        };
        const uint32_t n_final_object_name_parts = sizeof(final_object_name_parts) / sizeof(final_object_name_parts[0]);

        result = system_hashed_ansi_string_create_by_merging_strings(n_final_object_name_parts,
                                                                       final_object_name_parts);
    }
    else
    {
        static const char* limiter = "\\";

        const char* final_object_name_parts[] =
        {
            limiter,
            system_hashed_ansi_string_get_buffer(object_type_has),
            limiter,
            system_hashed_ansi_string_get_buffer(object_name)
        };
        const uint32_t n_final_object_name_parts = sizeof(final_object_name_parts) / sizeof(final_object_name_parts[0]);

        result = system_hashed_ansi_string_create_by_merging_strings(n_final_object_name_parts,
                                                                       final_object_name_parts);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API object_manager_directory object_manager_get_root_directory()
{
    return _root;
}

/** Please see header for specification */
PUBLIC void _object_manager_init()
{
    _root = object_manager_directory_create(system_hashed_ansi_string_create("") );

    /* Initialize HASes for all object types */
    _object_type_audio_stream_hashed_ansi_string                          = system_hashed_ansi_string_create("Audio Streams");
    _object_type_collada_data_hashed_ansi_string                          = system_hashed_ansi_string_create("COLLADA Data");
    _object_type_context_menu_hashed_ansi_string                          = system_hashed_ansi_string_create("Context Menus");
    _object_type_curve_container_hashed_ansi_string                       = system_hashed_ansi_string_create("Curves");
    _object_type_demo_flyby_hashed_ansi_string                            = system_hashed_ansi_string_create("Fly-by instances");
    _object_type_demo_timeline_hashed_ansi_string                         = system_hashed_ansi_string_create("Demo Timelines");
    _object_type_demo_timeline_segment_hashed_ansi_string                 = system_hashed_ansi_string_create("Demo Timeline Segment");
    _object_type_gfx_bfg_font_table_hashed_ansi_string                    = system_hashed_ansi_string_create("GFX BFG Font Tables");
    _object_type_gfx_image_hashed_ansi_string                             = system_hashed_ansi_string_create("GFX Images");
    _object_type_glsl_shader_constructor_hashed_ansi_string               = system_hashed_ansi_string_create("GLSL Shader Constructors");
    _object_type_mesh_hashed_ansi_string                                  = system_hashed_ansi_string_create("Meshes");
    _object_type_mesh_marchingcubes_hashed_ansi_string                    = system_hashed_ansi_string_create("Mesh (Marching-Cubes)");
    _object_type_mesh_material_hashed_ansi_string                         = system_hashed_ansi_string_create("Mesh Materials");
    _object_type_ocl_context_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenCL Contexts");
    _object_type_ocl_kdtree_hashed_ansi_string                            = system_hashed_ansi_string_create("OpenCL Kd Trees");
    _object_type_ocl_kernel_hashed_ansi_string                            = system_hashed_ansi_string_create("OpenCL Kernels");
    _object_type_ocl_program_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenCL Programs");
    _object_type_ogl_context_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenGL Contexts");
    _object_type_ogl_pipeline_hashed_ansi_string                          = system_hashed_ansi_string_create("OpenGL Pipelines");
    _object_type_ogl_rendering_handler_hashed_ansi_string                 = system_hashed_ansi_string_create("OpenGL Rendering Handlers");
    _object_type_ogl_textures_hashed_ansi_string                          = system_hashed_ansi_string_create("OpenGL Textures");
    _object_type_postprocessing_bloom_hashed_ansi_string                  = system_hashed_ansi_string_create("Post-processing Bloom");
    _object_type_postprocessing_blur_poisson_hashed_ansi_string           = system_hashed_ansi_string_create("Post-processing Blur Poisson");
    _object_type_postprocessing_motion_blur_hashed_ansi_string            = system_hashed_ansi_string_create("Post-processing Motion Blur");
    _object_type_postprocessing_reinhard_tonemap_hashed_ansi_string       = system_hashed_ansi_string_create("Post-processing Reinhard tonemap");
    _object_type_procedural_mesh_box_hashed_ansi_string                   = system_hashed_ansi_string_create("Procedural meshes (box)");
    _object_type_procedural_mesh_circle_hashed_ansi_string                = system_hashed_ansi_string_create("Procedural meshes (circle)");
    _object_type_procedural_mesh_sphere_hashed_ansi_string                = system_hashed_ansi_string_create("Procedural meshes (sphere)");
    _object_type_procedural_uv_generator_hashed_ansi_string               = system_hashed_ansi_string_create("Procedural UV Generators");
    _object_type_programs_curve_editor_curvebackground_hashed_ansi_string = system_hashed_ansi_string_create("Curve Editor Programs (Curve Background)");
    _object_type_programs_curve_editor_lerp_hashed_ansi_string            = system_hashed_ansi_string_create("Curve Editor Programs (LERP)");
    _object_type_programs_curve_editor_quadselector_hashed_ansi_string    = system_hashed_ansi_string_create("Curve Editor Programs (Quad Selector)");
    _object_type_programs_curve_editor_static_hashed_ansi_string          = system_hashed_ansi_string_create("Curve Editor Programs (Static)");
    _object_type_programs_curve_editor_tcb_hashed_ansi_string             = system_hashed_ansi_string_create("Curve Editor Programs (TCB)");
    _object_type_ragl_buffers_hashed_ansi_string                          = system_hashed_ansi_string_create("raGL Buffers");
    _object_type_ragl_sync_hashed_ansi_string                             = system_hashed_ansi_string_create("raGL Syncs");
    _object_type_ragl_textures_hashed_ansi_string                         = system_hashed_ansi_string_create("raGL Textures");
    _object_type_ral_context_hashed_ansi_string                           = system_hashed_ansi_string_create("RAL Contexts");
    _object_type_scalar_field_metaballs_hashed_ansi_string                = system_hashed_ansi_string_create("Scalar Field (Metaballs)");
    _object_type_scene_hashed_ansi_string                                 = system_hashed_ansi_string_create("Scenes");
    _object_type_scene_camera_hashed_ansi_string                          = system_hashed_ansi_string_create("Scene Cameras");
    _object_type_scene_curve_hashed_ansi_string                           = system_hashed_ansi_string_create("Scene Curves");
    _object_type_scene_light_hashed_ansi_string                           = system_hashed_ansi_string_create("Scene Lights");
    _object_type_scene_material_hashed_ansi_string                        = system_hashed_ansi_string_create("Scene Materials");
    _object_type_scene_mesh_hashed_ansi_string                            = system_hashed_ansi_string_create("Scene Mesh Instances");
    _object_type_scene_renderer_uber_hashed_ansi_string                   = system_hashed_ansi_string_create("Scene Renderer Ubers");
    _object_type_scene_surface_hashed_ansi_string                         = system_hashed_ansi_string_create("Scene Surfaces");
    _object_type_scene_texture_hashed_ansi_string                         = system_hashed_ansi_string_create("Scene Textures");
    _object_type_sh_projector_hashed_ansi_string                          = system_hashed_ansi_string_create("SH Projector");
    _object_type_sh_projector_combiner_hashed_ansi_string                 = system_hashed_ansi_string_create("SH Projector Combiner");
    _object_type_sh_rot_hashed_ansi_string                                = system_hashed_ansi_string_create("SH Rotations");
    _object_type_sh_samples_hashed_ansi_string                            = system_hashed_ansi_string_create("SH Sample Generator");
    _object_type_sh_shaders_object_hashed_ansi_string                     = system_hashed_ansi_string_create("SH Shader Objects");
    _object_type_shaders_fragment_rgb_to_Yxy                              = system_hashed_ansi_string_create("RGB to Yxy Shaders");
    _object_type_shaders_fragment_static                                  = system_hashed_ansi_string_create("Static Fragment Shaders");
    _object_type_shaders_fragment_texture2D_filmic                        = system_hashed_ansi_string_create("Texture2D Filmic Fragment Shaders");
    _object_type_shaders_fragment_texture2D_filmic_customizable           = system_hashed_ansi_string_create("Texture2D Customizable Filmic Fragment Shaders");
    _object_type_shaders_fragment_texture2D_linear                        = system_hashed_ansi_string_create("Texture2D Linear Fragment Shaders");
    _object_type_shaders_fragment_texture2D_plain                         = system_hashed_ansi_string_create("Texture2D Plain Fragment Shaders");
    _object_type_shaders_fragment_texture2D_reinhardt                     = system_hashed_ansi_string_create("Texture2D Reinhardt Fragment Shaders");
    _object_type_shaders_fragment_uber                                    = system_hashed_ansi_string_create("Uber Fragment Shaders");
    _object_type_shaders_fragment_Yxy_to_rgb                              = system_hashed_ansi_string_create("Yxy to RGB Shaders");
    _object_type_shaders_vertex_combinedmvp_generic                       = system_hashed_ansi_string_create("Combined MVP Generic Vertex Shaders");
    _object_type_shaders_vertex_combinedmvp_simplified_two_point          = system_hashed_ansi_string_create("Combined MVP Simplified 2-point Vertex Shaders");
    _object_type_shaders_vertex_fullscreen                                = system_hashed_ansi_string_create("Full-screen Vertex Shaders");
    _object_type_shaders_vertex_uber                                      = system_hashed_ansi_string_create("Uber Vertex Shaders");
    _object_type_system_file_serializer_hashed_ansi_string                = system_hashed_ansi_string_create("File Serializers");
    _object_type_system_randomizer_hashed_ansi_string                     = system_hashed_ansi_string_create("System Randomizers");
    _object_type_system_window_hashed_ansi_string                         = system_hashed_ansi_string_create("System Windows"); 
    _object_type_ui_hashed_ansi_string                                    = system_hashed_ansi_string_create("UIs");
    _object_type_unknown_hashed_ansi_string                               = system_hashed_ansi_string_create("Unknown");
    _object_type_varia_curve_renderer_hashed_ansi_string                  = system_hashed_ansi_string_create("Varia Curve Renderers");
    _object_type_varia_primitive_renderer_hashed_ansi_string              = system_hashed_ansi_string_create("Varia Primitive Renderers");
    _object_type_varia_skybox_hashed_ansi_string                          = system_hashed_ansi_string_create("Varia Skyboxes");
    _object_type_varia_text_renderer_hashed_ansi_string                   = system_hashed_ansi_string_create("Varia Text Renderers");

    /* Create "directories" (one per each object type) in the internal registry. */
    for (object_manager_object_type object_type  = OBJECT_TYPE_FIRST;
                                    object_type != OBJECT_TYPE_LAST;
                             ((int&)object_type)++)
    {
        system_hashed_ansi_string subdirectory_name = object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_type);
        object_manager_directory  subdirectory      = object_manager_directory_create                                        (subdirectory_name);

        if (!object_manager_directory_insert_subdirectory(_root,
                                                          subdirectory) )
        {
            LOG_ERROR("Could not insert directory [%s] into root",
                      system_hashed_ansi_string_get_buffer(subdirectory_name) );
        }
    }
}

/** Please see header for specification */
PUBLIC void _object_manager_register_refcounted_object(void*                      ptr,
                                                       system_hashed_ansi_string  item_registration_path,
                                                       const char*                source_code_file_name,
                                                       int                        source_code_file_line,
                                                       object_manager_object_type object_type)
{
    system_hashed_ansi_string item_name         = NULL;
    system_hashed_ansi_string item_path         = NULL;
    bool                      extraction_result = object_manager_directory_extract_item_name_and_path(item_registration_path,
                                                                                                     &item_path,
                                                                                                     &item_name);

    ASSERT_DEBUG_SYNC(extraction_result,
                      "Invalid registration path for an object! [%s]",
                      system_hashed_ansi_string_get_buffer(item_registration_path) );

    if (extraction_result)
    {
        object_manager_directory parent_directory = object_manager_directory_find_subdirectory(_root,
                                                                                               item_path);

        /* If the directory we are to register the item at is unavailable, create it */
        if (parent_directory == NULL)
        {
            object_manager_directory_create_directory_structure(_root,
                                                                item_path);

            parent_directory = object_manager_directory_find_subdirectory_recursive(_root,
                                                                                    item_path);
        }

        ASSERT_DEBUG_SYNC(parent_directory != NULL,
                          "Could not find parent directory for an object! [%s]",
                          system_hashed_ansi_string_get_buffer(item_registration_path) );

        if (parent_directory != NULL)
        {
            /* Instantiate item descriptor */
            object_manager_item item = object_manager_item_create(item_name,
                                                                  system_hashed_ansi_string_create(source_code_file_name),
                                                                  source_code_file_line,
                                                                  object_type,
                                                                  ptr);

            ASSERT_DEBUG_SYNC(item != NULL,
                              "Could not create item of type [%d]",
                              object_type);

            if (item != NULL)
            {
                /* Try to insert the item into storage */
                bool insert_result = object_manager_directory_insert_item(parent_directory,
                                                                          item);

                if (!insert_result)
                {
                    LOG_ERROR("Could not insert refcounted object descriptor");

                    object_manager_item_release(item);
                    item = NULL;
                }
            }
        }
    }
}

/** Please see header for specification */
PUBLIC void _object_manager_unregister_refcounted_object(system_hashed_ansi_string item_registration_path)
{
    system_hashed_ansi_string item_name = NULL;
    system_hashed_ansi_string item_path = NULL;

    if (object_manager_directory_extract_item_name_and_path(item_registration_path,
                                                           &item_path,
                                                           &item_name) )
    {
        const char* item_path_ptr = system_hashed_ansi_string_get_buffer(item_path);

        system_hashed_ansi_string item_registration_path_modified = system_hashed_ansi_string_create                    (item_path_ptr);
        object_manager_directory  item_directory                  = object_manager_directory_find_subdirectory_recursive(_root,
                                                                                                                         item_registration_path_modified);

        if (item_directory != NULL)
        {
            object_manager_item item = object_manager_directory_find_item(item_directory,
                                                                          item_name);

            if (item != NULL)
            {
                /* Before releasing, remove the object from the storage */
                if (!object_manager_directory_delete_item(item_directory,
                                                          item_name) )
                {
                    LOG_ERROR        ("Could not remove item [%s] from registry, will release it anyway.",
                                      system_hashed_ansi_string_get_buffer(item_registration_path) );

                    ASSERT_DEBUG_SYNC(false,
                                      "");
                }
            }
            else
            {
                LOG_ERROR        ("Even though item at [%s] was found, it corresponds to NULL which is WRONG",
                                  system_hashed_ansi_string_get_buffer(item_registration_path) );

                ASSERT_DEBUG_SYNC(false,
                                  "");
            }
        }
        else
        {
            LOG_ERROR        ("Could not find item at [%s] for unregistration!",
                              system_hashed_ansi_string_get_buffer(item_registration_path) );

            ASSERT_DEBUG_SYNC(false,
                              "");
        }
    }
    else
    {
        LOG_ERROR        ("Could not extract item/path for registration path [%s]",
                          system_hashed_ansi_string_get_buffer(item_registration_path) );

        ASSERT_DEBUG_SYNC(false,
                          "");
    }
}