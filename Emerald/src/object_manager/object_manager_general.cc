/**
 *
 * Emerald (kbi/elude @2012)
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

static const system_hashed_ansi_string _object_type_collada_data_hashed_ansi_string                          = system_hashed_ansi_string_create("COLLADA Data");
static const system_hashed_ansi_string _object_type_context_menu_hashed_ansi_string                          = system_hashed_ansi_string_create("Context Menus");
static const system_hashed_ansi_string _object_type_curve_container_hashed_ansi_string                       = system_hashed_ansi_string_create("Curves");
static const system_hashed_ansi_string _object_type_gfx_bfg_font_table_hashed_ansi_string                    = system_hashed_ansi_string_create("GFX BFG Font Tables");
static const system_hashed_ansi_string _object_type_gfx_image_hashed_ansi_string                             = system_hashed_ansi_string_create("GFX Images");
static const system_hashed_ansi_string _object_type_lw_curve_dataset_hashed_ansi_string                      = system_hashed_ansi_string_create("Lightwave curve data-sets");
static const system_hashed_ansi_string _object_type_lw_dataset_hashed_ansi_string                            = system_hashed_ansi_string_create("Lightwave data-sets");
static const system_hashed_ansi_string _object_type_lw_light_dataset_hashed_ansi_string                      = system_hashed_ansi_string_create("Lightwave light data-sets");
static const system_hashed_ansi_string _object_type_lw_material_dataset_hashed_ansi_string                   = system_hashed_ansi_string_create("Lightwave material data-sets");
static const system_hashed_ansi_string _object_type_lw_mesh_dataset_hashed_ansi_string                       = system_hashed_ansi_string_create("Lightwave mesh data-sets");
static const system_hashed_ansi_string _object_type_mesh_hashed_ansi_string                                  = system_hashed_ansi_string_create("Meshes");
static const system_hashed_ansi_string _object_type_mesh_material_hashed_ansi_string                         = system_hashed_ansi_string_create("Mesh Materials");
static const system_hashed_ansi_string _object_type_ocl_context_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenCL Contexts");
static const system_hashed_ansi_string _object_type_ocl_kdtree_hashed_ansi_string                            = system_hashed_ansi_string_create("OpenCL Kd Trees");
static const system_hashed_ansi_string _object_type_ocl_kernel_hashed_ansi_string                            = system_hashed_ansi_string_create("OpenCL Kernels");
static const system_hashed_ansi_string _object_type_ocl_program_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenCL Programs");
static const system_hashed_ansi_string _object_type_ogl_context_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenGL Contexts");
static const system_hashed_ansi_string _object_type_ogl_curve_renderer_hashed_ansi_string                    = system_hashed_ansi_string_create("Curve Renderers");
static const system_hashed_ansi_string _object_type_ogl_pipeline_hashed_ansi_string                          = system_hashed_ansi_string_create("OpenGL Pipelines");
static const system_hashed_ansi_string _object_type_ogl_pixel_format_descriptor_hashed_ansi_string           = system_hashed_ansi_string_create("OpenGL Pixel Format Descriptors");
static const system_hashed_ansi_string _object_type_ogl_primitive_renderer_hashed_ansi_string                = system_hashed_ansi_string_create("Primitive Renderers");
static const system_hashed_ansi_string _object_type_ogl_program_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenGL Programs");
static const system_hashed_ansi_string _object_type_ogl_rendering_handler_hashed_ansi_string                 = system_hashed_ansi_string_create("OpenGL Rendering Handlers");
static const system_hashed_ansi_string _object_type_ogl_sampler_hashed_ansi_string                           = system_hashed_ansi_string_create("OpenGL Samplers");
static const system_hashed_ansi_string _object_type_ogl_shader_constructor_hashed_ansi_string                = system_hashed_ansi_string_create("OpenGL Shader Constructors");
static const system_hashed_ansi_string _object_type_ogl_shader_hashed_ansi_string                            = system_hashed_ansi_string_create("OpenGL Shaders");
static const system_hashed_ansi_string _object_type_ogl_skybox_hashed_ansi_string                            = system_hashed_ansi_string_create("OpenGL Skyboxes");
static const system_hashed_ansi_string _object_type_ogl_text_hashed_ansi_string                              = system_hashed_ansi_string_create("OpenGL Text Renderers");
static const system_hashed_ansi_string _object_type_ogl_textures_hashed_ansi_string                          = system_hashed_ansi_string_create("OpenGL Textures");
static const system_hashed_ansi_string _object_type_ogl_uber_hashed_ansi_string                              = system_hashed_ansi_string_create("OpenGL Ubers");
static const system_hashed_ansi_string _object_type_ogl_ui_hashed_ansi_string                                = system_hashed_ansi_string_create("OpenGL UIs");
static const system_hashed_ansi_string _object_type_postprocessing_bloom_hashed_ansi_string                  = system_hashed_ansi_string_create("Post-processing Bloom");
static const system_hashed_ansi_string _object_type_postprocessing_blur_poisson_hashed_ansi_string           = system_hashed_ansi_string_create("Post-processing Blur Poisson");
static const system_hashed_ansi_string _object_type_postprocessing_reinhard_tonemap_hashed_ansi_string       = system_hashed_ansi_string_create("Post-processing Reinhard tonemap");
static const system_hashed_ansi_string _object_type_procedural_mesh_box_hashed_ansi_string                   = system_hashed_ansi_string_create("Procedural meshes (box)");
static const system_hashed_ansi_string _object_type_procedural_mesh_sphere_hashed_ansi_string                = system_hashed_ansi_string_create("Procedural meshes (sphere)");
static const system_hashed_ansi_string _object_type_programs_curve_editor_curvebackground_hashed_ansi_string = system_hashed_ansi_string_create("Curve Editor Programs (Curve Background)");
static const system_hashed_ansi_string _object_type_programs_curve_editor_lerp_hashed_ansi_string            = system_hashed_ansi_string_create("Curve Editor Programs (LERP)");
static const system_hashed_ansi_string _object_type_programs_curve_editor_quadselector_hashed_ansi_string    = system_hashed_ansi_string_create("Curve Editor Programs (Quad Selector)");
static const system_hashed_ansi_string _object_type_programs_curve_editor_static_hashed_ansi_string          = system_hashed_ansi_string_create("Curve Editor Programs (Static)");
static const system_hashed_ansi_string _object_type_programs_curve_editor_tcb_hashed_ansi_string             = system_hashed_ansi_string_create("Curve Editor Programs (TCB)");
static const system_hashed_ansi_string _object_type_scene_hashed_ansi_string                                 = system_hashed_ansi_string_create("Scenes");
static const system_hashed_ansi_string _object_type_scene_camera_hashed_ansi_string                          = system_hashed_ansi_string_create("Scene Cameras");
static const system_hashed_ansi_string _object_type_scene_curve_hashed_ansi_string                           = system_hashed_ansi_string_create("Scene Curves");
static const system_hashed_ansi_string _object_type_scene_light_hashed_ansi_string                           = system_hashed_ansi_string_create("Scene Lights");
static const system_hashed_ansi_string _object_type_scene_mesh_hashed_ansi_string                            = system_hashed_ansi_string_create("Scene Mesh Instances");
static const system_hashed_ansi_string _object_type_scene_surface_hashed_ansi_string                         = system_hashed_ansi_string_create("Scene Surfaces");
static const system_hashed_ansi_string _object_type_scene_texture_hashed_ansi_string                         = system_hashed_ansi_string_create("Scene Textures");
static const system_hashed_ansi_string _object_type_sh_projector_hashed_ansi_string                          = system_hashed_ansi_string_create("SH Projector");
static const system_hashed_ansi_string _object_type_sh_projector_combiner_hashed_ansi_string                 = system_hashed_ansi_string_create("SH Projector Combiner");
static const system_hashed_ansi_string _object_type_sh_rot_hashed_ansi_string                                = system_hashed_ansi_string_create("SH Rotations");
static const system_hashed_ansi_string _object_type_sh_samples_hashed_ansi_string                            = system_hashed_ansi_string_create("SH Sample Generator");
static const system_hashed_ansi_string _object_type_sh_shaders_object_hashed_ansi_string                     = system_hashed_ansi_string_create("SH Shader Objects");
static const system_hashed_ansi_string _object_type_shaders_fragment_rgb_to_Yxy                              = system_hashed_ansi_string_create("RGB to Yxy Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_static                                  = system_hashed_ansi_string_create("Static Fragment Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_texture2D_filmic                        = system_hashed_ansi_string_create("Texture2D Filmic Fragment Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_texture2D_filmic_customizable           = system_hashed_ansi_string_create("Texture2D Customizable Filmic Fragment Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_texture2D_linear                        = system_hashed_ansi_string_create("Texture2D Linear Fragment Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_texture2D_plain                         = system_hashed_ansi_string_create("Texture2D Plain Fragment Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_texture2D_reinhardt                     = system_hashed_ansi_string_create("Texture2D Reinhardt Fragment Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_uber                                    = system_hashed_ansi_string_create("Uber Fragment Shaders");
static const system_hashed_ansi_string _object_type_shaders_fragment_Yxy_to_rgb                              = system_hashed_ansi_string_create("Yxy to RGB Shaders");
static const system_hashed_ansi_string _object_type_shaders_vertex_combinedmvp_generic                       = system_hashed_ansi_string_create("Combined MVP Generic Vertex Shaders");
static const system_hashed_ansi_string _object_type_shaders_vertex_combinedmvp_simplified_two_point          = system_hashed_ansi_string_create("Combined MVP Simplified 2-point Vertex Shaders");
static const system_hashed_ansi_string _object_type_shaders_vertex_combinedmvp_ubo                           = system_hashed_ansi_string_create("Combiend MVP UBO Vertex Shaders");
static const system_hashed_ansi_string _object_type_shaders_vertex_fullscreen                                = system_hashed_ansi_string_create("Full-screen Vertex Shaders");
static const system_hashed_ansi_string _object_type_shaders_vertex_m_vp_generic                              = system_hashed_ansi_string_create("M+VP Generic Vertex Shaders");
static const system_hashed_ansi_string _object_type_shaders_vertex_mvp_combiner                              = system_hashed_ansi_string_create("MVP Combiner Vertex Shaders");
static const system_hashed_ansi_string _object_type_shaders_vertex_uber                                      = system_hashed_ansi_string_create("Uber Vertex Shaders");
static const system_hashed_ansi_string _object_type_system_randomizer_hashed_ansi_string                     = system_hashed_ansi_string_create("System Randomizers");
static const system_hashed_ansi_string _object_type_system_window_hashed_ansi_string                         = system_hashed_ansi_string_create("System Windows"); 
static const system_hashed_ansi_string _object_type_unknown_hashed_ansi_string                               = system_hashed_ansi_string_create("Unknown");


/** Please see header for specification */
PUBLIC system_hashed_ansi_string object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_manager_object_type object_type)
{
    switch (object_type)
    {
        case OBJECT_TYPE_COLLADA_DATA:                                    return _object_type_collada_data_hashed_ansi_string;
        case OBJECT_TYPE_CONTEXT_MENU:                                    return _object_type_context_menu_hashed_ansi_string;
        case OBJECT_TYPE_CURVE_CONTAINER:                                 return _object_type_curve_container_hashed_ansi_string;
        case OBJECT_TYPE_GFX_BFG_FONT_TABLE:                              return _object_type_gfx_bfg_font_table_hashed_ansi_string;
        case OBJECT_TYPE_GFX_IMAGE:                                       return _object_type_gfx_image_hashed_ansi_string;
        case OBJECT_TYPE_LW_CURVE_DATASET:                                return _object_type_lw_curve_dataset_hashed_ansi_string;
        case OBJECT_TYPE_LW_DATASET:                                      return _object_type_lw_dataset_hashed_ansi_string;
        case OBJECT_TYPE_LW_LIGHT_DATASET:                                return _object_type_lw_light_dataset_hashed_ansi_string;
        case OBJECT_TYPE_LW_MATERIAL_DATASET:                             return _object_type_lw_material_dataset_hashed_ansi_string;
        case OBJECT_TYPE_LW_MESH_DATASET:                                 return _object_type_lw_mesh_dataset_hashed_ansi_string;
        case OBJECT_TYPE_MESH:                                            return _object_type_mesh_hashed_ansi_string;
        case OBJECT_TYPE_MESH_MATERIAL:                                   return _object_type_mesh_material_hashed_ansi_string;
        case OBJECT_TYPE_OCL_CONTEXT:                                     return _object_type_ocl_context_hashed_ansi_string;
        case OBJECT_TYPE_OCL_KDTREE:                                      return _object_type_ocl_kdtree_hashed_ansi_string;
        case OBJECT_TYPE_OCL_KERNEL:                                      return _object_type_ocl_kernel_hashed_ansi_string;
        case OBJECT_TYPE_OCL_PROGRAM:                                     return _object_type_ocl_program_hashed_ansi_string;
        case OBJECT_TYPE_OGL_CONTEXT:                                     return _object_type_ogl_context_hashed_ansi_string;
        case OBJECT_TYPE_OGL_CURVE_RENDERER:                              return _object_type_ogl_curve_renderer_hashed_ansi_string;
        case OBJECT_TYPE_OGL_PIPELINE:                                    return _object_type_ogl_pipeline_hashed_ansi_string;
        case OBJECT_TYPE_OGL_PIXEL_FORMAT_DESCRIPTOR:                     return _object_type_ogl_pixel_format_descriptor_hashed_ansi_string;
        case OBJECT_TYPE_OGL_PRIMITIVE_RENDERER:                          return _object_type_ogl_primitive_renderer_hashed_ansi_string;
        case OBJECT_TYPE_OGL_PROGRAM:                                     return _object_type_ogl_program_hashed_ansi_string;
        case OBJECT_TYPE_OGL_RENDERING_HANDLER:                           return _object_type_ogl_rendering_handler_hashed_ansi_string;
        case OBJECT_TYPE_OGL_SAMPLER:                                     return _object_type_ogl_sampler_hashed_ansi_string;
        case OBJECT_TYPE_OGL_SHADER:                                      return _object_type_ogl_shader_hashed_ansi_string;
        case OBJECT_TYPE_OGL_SHADER_CONSTRUCTOR:                          return _object_type_ogl_shader_constructor_hashed_ansi_string;
        case OBJECT_TYPE_OGL_SKYBOX:                                      return _object_type_ogl_skybox_hashed_ansi_string;
        case OBJECT_TYPE_OGL_TEXT:                                        return _object_type_ogl_text_hashed_ansi_string;
        case OBJECT_TYPE_OGL_TEXTURE:                                     return _object_type_ogl_textures_hashed_ansi_string;
        case OBJECT_TYPE_OGL_UBER:                                        return _object_type_ogl_uber_hashed_ansi_string;
        case OBJECT_TYPE_OGL_UI:                                          return _object_type_ogl_ui_hashed_ansi_string;
        case OBJECT_TYPE_POSTPROCESSING_BLOOM:                            return _object_type_postprocessing_bloom_hashed_ansi_string;
        case OBJECT_TYPE_POSTPROCESSING_BLUR_POISSON:                     return _object_type_postprocessing_blur_poisson_hashed_ansi_string;
        case OBJECT_TYPE_POSTPROCESSING_REINHARD_TONEMAP:                 return _object_type_postprocessing_reinhard_tonemap_hashed_ansi_string;
        case OBJECT_TYPE_PROCEDURAL_MESH_BOX:                             return _object_type_procedural_mesh_box_hashed_ansi_string;
        case OBJECT_TYPE_PROCEDURAL_MESH_SPHERE:                          return _object_type_procedural_mesh_sphere_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_CURVEBACKGROUND:           return _object_type_programs_curve_editor_curvebackground_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_LERP:                      return _object_type_programs_curve_editor_lerp_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_QUADSELECTOR:              return _object_type_programs_curve_editor_quadselector_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_STATIC:                    return _object_type_programs_curve_editor_static_hashed_ansi_string;
        case OBJECT_TYPE_PROGRAMS_CURVE_EDITOR_TCB:                       return _object_type_programs_curve_editor_tcb_hashed_ansi_string;
        case OBJECT_TYPE_SCENE:                                           return _object_type_scene_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_CAMERA:                                    return _object_type_scene_camera_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_CURVE:                                     return _object_type_scene_curve_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_LIGHT:                                     return _object_type_scene_light_hashed_ansi_string;
        case OBJECT_TYPE_SCENE_MESH:                                      return _object_type_scene_mesh_hashed_ansi_string;
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
        case OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_UBO:                  return _object_type_shaders_vertex_combinedmvp_ubo;
        case OBJECT_TYPE_SHADERS_VERTEX_M_VP_GENERIC:                     return _object_type_shaders_vertex_m_vp_generic;
        case OBJECT_TYPE_SHADERS_VERTEX_MVP_COMBINER:                     return _object_type_shaders_vertex_mvp_combiner;
        case OBJECT_TYPE_SHADERS_VERTEX_UBER:                             return _object_type_shaders_vertex_uber;
        case OBJECT_TYPE_SYSTEM_RANDOMIZER:                               return _object_type_system_randomizer_hashed_ansi_string;
        case OBJECT_TYPE_SYSTEM_WINDOW:                                   return _object_type_system_window_hashed_ansi_string;
        default:                                                          return _object_type_unknown_hashed_ansi_string;
    }

}

/** TODO */
PRIVATE void _object_manager_delete_empty_subdirectories(object_manager_directory directory)
{
    uint32_t n_subdirectories = object_manager_directory_get_amount_of_subdirectories_for_directory(directory);

    if (n_subdirectories != 0)
    {
        system_resizable_vector subdirectories = system_resizable_vector_create(n_subdirectories, sizeof(object_manager_directory) );

        /* Cache subdirectory names */
        for (uint32_t n = 0; n < n_subdirectories; ++n)
        {
            object_manager_directory subdirectory = object_manager_directory_get_subdirectory_at(directory, n);

            ASSERT_DEBUG_SYNC(subdirectory != NULL, "Subdirectory cannot be null.");
            system_resizable_vector_push(subdirectories, subdirectory);
        }

        /* Iterate through cached subdirectory names and recursively call the function */
        while (system_resizable_vector_get_amount_of_elements(subdirectories) != 0)
        {
            object_manager_directory subdirectory = NULL;
            bool                     result       = system_resizable_vector_pop(subdirectories, &subdirectory);

            ASSERT_DEBUG_SYNC(result, "Could not pop subdirectory from internal cache.");
            if (result)
            {
                _object_manager_delete_empty_subdirectories(subdirectory);

                if (object_manager_directory_get_amount_of_children_for_directory(subdirectory) == 0)
                {
                    object_manager_directory_delete_directory(directory, object_manager_directory_get_name(subdirectory) );
                }
            }
        }

        system_resizable_vector_release(subdirectories);
    }
}


/** Please see header for specification */
PUBLIC void _object_manager_deinit()
{
    /* If default directories are still there, remove them if they contain no elements */
    for (object_manager_object_type object_type = OBJECT_TYPE_FIRST; object_type != OBJECT_TYPE_LAST; ((int&)object_type)++)
    {
        system_hashed_ansi_string subdirectory_name = object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_type);
        object_manager_directory  subdirectory      = object_manager_directory_find_subdirectory(_root, subdirectory_name);

        if (subdirectory != NULL)
        {
            _object_manager_delete_empty_subdirectories(subdirectory);

            if (object_manager_directory_get_amount_of_children_for_directory(subdirectory) == 0)
            {
                if (!object_manager_directory_delete_directory(_root, subdirectory_name) )
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
        LOG_ERROR("Resource leak detected upon exiting! %d elements are still there!", n_elements);
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
PUBLIC EMERALD_API object_manager_directory object_manager_get_root_directory()
{
    return _root;
}

/** Please see header for specification */
PUBLIC void _object_manager_init()
{
    _root = object_manager_directory_create(system_hashed_ansi_string_create("") );

    for (object_manager_object_type object_type = OBJECT_TYPE_FIRST; object_type != OBJECT_TYPE_LAST; ((int&)object_type)++)
    {
        system_hashed_ansi_string subdirectory_name = object_manager_convert_object_manager_object_type_to_hashed_ansi_string(object_type);
        object_manager_directory  subdirectory      = object_manager_directory_create(subdirectory_name);

        if (!object_manager_directory_insert_subdirectory(_root, subdirectory) )
        {
            LOG_ERROR("Could not insert directory [%s] into root", system_hashed_ansi_string_get_buffer(subdirectory_name) );
        }
    }
}

/** Please see header for specification */
PUBLIC void _object_manager_register_refcounted_object(void* ptr, system_hashed_ansi_string item_registration_path, const char* source_code_file_name, int source_code_file_line, object_manager_object_type object_type)
{
    system_hashed_ansi_string item_name         = NULL;
    system_hashed_ansi_string item_path         = NULL;
    bool                      extraction_result = object_manager_directory_extract_item_name_and_path(item_registration_path, &item_path, &item_name);

    ASSERT_DEBUG_SYNC(extraction_result, "Invalid registration path for an object! [%s]", system_hashed_ansi_string_get_buffer(item_registration_path) );
    if (extraction_result)
    {
        object_manager_directory parent_directory = object_manager_directory_find_subdirectory(_root, item_path);

        /* If the directory we are to register the item at is unavailable, create it */
        if (parent_directory == NULL)
        {
            object_manager_directory_create_directory_structure(_root, item_path);

            parent_directory = object_manager_directory_find_subdirectory_recursive(_root, item_path);
        }

        ASSERT_DEBUG_SYNC(parent_directory != NULL, "Could not find parent directory for an object! [%s]", system_hashed_ansi_string_get_buffer(item_registration_path) );
        if (parent_directory != NULL)
        {
            /* Instantiate item descriptor */
            object_manager_item item = object_manager_item_create(item_name,
                                                                  system_hashed_ansi_string_create(source_code_file_name),
                                                                  source_code_file_line,
                                                                  object_type,
                                                                  ptr);

            ASSERT_DEBUG_SYNC(item != NULL, "Could not create item of type [%d]", object_type);
            if (item != NULL)
            {
                /* Try to insert the item into storage */
                bool insert_result = object_manager_directory_insert_item(parent_directory, item);

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

#include <crtdbg.h>

/** Please see header for specification */
PUBLIC void _object_manager_unregister_refcounted_object(system_hashed_ansi_string item_registration_path)
{
    system_hashed_ansi_string item_name = NULL;
    system_hashed_ansi_string item_path = NULL;

    if (object_manager_directory_extract_item_name_and_path(item_registration_path, &item_path, &item_name) )
    {
        const char* item_path_ptr = system_hashed_ansi_string_get_buffer(item_path);

        system_hashed_ansi_string item_registration_path_modified = system_hashed_ansi_string_create(item_path_ptr);
        object_manager_directory  item_directory                  = object_manager_directory_find_subdirectory_recursive(_root, item_registration_path_modified);

        if (item_directory != NULL)
        {
            object_manager_item item = object_manager_directory_find_item(item_directory, item_name);

            if (item != NULL)
            {
                /* Before releasing, remove the object from the storage */
                if (!object_manager_directory_delete_item(item_directory, item_name) )
                {
                    LOG_ERROR        ("Could not remove item [%s] from registry, will release it anyway.", system_hashed_ansi_string_get_buffer(item_registration_path) );
                    ASSERT_DEBUG_SYNC(false, "");
                }
            }
            else
            {
                LOG_ERROR        ("Even though item at [%s] was found, it corresponds to NULL which is WRONG", system_hashed_ansi_string_get_buffer(item_registration_path) );
                ASSERT_DEBUG_SYNC(false, "");
            }
        }
        else
        {
            LOG_ERROR        ("Could not find item at [%s] for unregistration!", system_hashed_ansi_string_get_buffer(item_registration_path) );
            ASSERT_DEBUG_SYNC(false, "");
        }
    }
    else
    {
        LOG_ERROR        ("Could not extract item/path for registration path [%s]", system_hashed_ansi_string_get_buffer(item_registration_path) );
        ASSERT_DEBUG_SYNC(false, "");
    }
}