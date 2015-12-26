/**
 *
 * DOF test app.
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_background.h"
#include "stage_step_julia.h"
#include "gfx/gfx_image.h"
#include "gfx/gfx_rgbe.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_skybox.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_texture.h"
#include "system/system_matrix4x4.h"

ral_framebuffer  _fbo            = NULL;
system_matrix4x4 _inv_projection = NULL;
system_matrix4x4 _mv             = NULL;
ral_texture      _result_texture = NULL;
ogl_skybox       _skybox         = NULL;
gfx_image        _skybox_image   = NULL;
ral_texture      _skybox_texture = NULL;

/** TODO */
static void _stage_step_background_execute(ral_context context,
                                           uint32_t    frame_index,
                                           system_time time,
                                           const int*  rendering_area_px_topdown,
                                           void*       not_used)
{
    ogl_context                 context_gl        = NULL;
    ogl_context_gl_entrypoints* entrypoints_ptr   = NULL;
    raGL_framebuffer            fbo_raGL          = NULL;
    GLuint                      fbo_raGL_id       = 0;
    ogl_flyby                   flyby             = NULL;
    system_matrix4x4            projection_matrix = main_get_projection_matrix();

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    fbo_raGL = ral_context_get_framebuffer_gl(context,
                                              _fbo);

    raGL_framebuffer_get_property(fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &fbo_raGL_id);


    system_matrix4x4_set_from_matrix4x4(_inv_projection,
                                        projection_matrix);
    system_matrix4x4_invert            (_inv_projection);


    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        fbo_raGL_id);

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_FLYBY,
                            &flyby);
    ogl_flyby_get_property  (flyby,
                             OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                            &_mv);

    ogl_skybox_draw(_skybox,
                    _mv,
                    _inv_projection);
}

/* Please see header for specification */
PUBLIC void stage_step_background_deinit(ral_context context)
{
    ral_texture textures_to_release[] =
    {
        _result_texture,
        _skybox_texture
    };
    const uint32_t n_textures_to_release = sizeof(textures_to_release) / sizeof(textures_to_release[0]);

    system_matrix4x4_release(_inv_projection);
    system_matrix4x4_release(_mv);

    ral_context_delete_framebuffers(context,
                                    1, /* n_framebuffers */
                                   &_fbo);
    ral_context_delete_textures    (context,
                                    n_textures_to_release,
                                    textures_to_release);

    ogl_skybox_release (_skybox);
    gfx_image_release  (_skybox_image);
}

/* Please see header for specification */
PUBLIC ral_texture stage_step_background_get_background_texture()
{
    return _skybox_texture;
}

/* Please see header for specification */
PUBLIC ral_texture stage_step_background_get_result_texture()
{
    return _result_texture;
}

/* Please see header for specification */
PUBLIC void stage_step_background_init(ral_context  context,
                                       ogl_pipeline pipeline,
                                       uint32_t     stage_id)
{
    ogl_context                                               context_gl             = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints        = NULL;
    raGL_texture                                              result_texture_raGL    = NULL;
    GLuint                                                    result_texture_raGL_id = 0;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);

    /* Initialize matrix instance */
    _inv_projection = system_matrix4x4_create();
    _mv             = system_matrix4x4_create();

    /* Initialize result texture */
    ral_texture_create_info result_texture_create_info;

    result_texture_create_info.base_mipmap_depth      = 1;
    result_texture_create_info.base_mipmap_height     = main_get_output_resolution()[1];
    result_texture_create_info.base_mipmap_width      = main_get_output_resolution()[0];
    result_texture_create_info.fixed_sample_locations = true;
    result_texture_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA32_FLOAT;
    result_texture_create_info.name                   = system_hashed_ansi_string_create("BG result texture");
    result_texture_create_info.n_layers               = 1;
    result_texture_create_info.n_samples              = 1;
    result_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    result_texture_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                        RAL_TEXTURE_USAGE_SAMPLED_BIT;
    result_texture_create_info.use_full_mipmap_chain  = false;

    ral_context_create_textures(context,
                                1, /* n_textures */
                               &result_texture_create_info,
                               &_result_texture);

    result_texture_raGL = ral_context_get_texture_gl(context,
                                                    _result_texture);

    raGL_texture_get_property(result_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &result_texture_raGL_id);

    dsa_entrypoints->pGLTextureParameteriEXT(result_texture_raGL_id,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_MIN_FILTER,
                                             GL_LINEAR);

    /* Initialize FBO */
    ral_context_create_framebuffers  (context,
                                      1, /* n_framebuffers */
                                     &_fbo);
    ral_framebuffer_set_attachment_2D(_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0, /* index */
                                      _result_texture,
                                      0 /* n_mipmap */);

    /* Initialize skybox data */
    _skybox_image = gfx_image_create_from_file(system_hashed_ansi_string_create("Skybox image"),
                                               system_hashed_ansi_string_create("galileo_probe.hdr"),
                                               false /* use_alternative_filename_getter */);

    ral_context_create_textures_from_gfx_images(context,
                                                1, /* n_images */
                                               &_skybox_image,
                                               &_skybox_texture);

    _skybox = ogl_skybox_create_spherical_projection_texture(context,
                                                             _skybox_texture,
                                                             system_hashed_ansi_string_create("skybox") );

    /* Add ourselves to the pipeline */
    ogl_pipeline_stage_step_declaration stage_step_background;

    stage_step_background.name              = system_hashed_ansi_string_create("Background");
    stage_step_background.pfn_callback_proc = _stage_step_background_execute;
    stage_step_background.user_arg          = NULL;

    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                               &stage_step_background);
}
