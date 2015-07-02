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
#include "ogl/ogl_texture.h"
#include "system/system_matrix4x4.h"

GLuint           _fbo_id         = 0;
system_matrix4x4 _inv_projection = NULL;
system_matrix4x4 _mv             = NULL;
ogl_texture      _result_texture = NULL;
ogl_skybox       _skybox         = NULL;
gfx_image        _skybox_image   = NULL;
ogl_texture      _skybox_texture = NULL;

/** TODO */
static void _stage_step_background_execute(ogl_context          context,
                                           system_timeline_time time,
                                           void*                not_used)
{
    const GLenum                      draw_buffer       = GL_COLOR_ATTACHMENT0;
    const ogl_context_gl_entrypoints* entrypoints       = NULL;
    ogl_flyby                         flyby             = NULL;
    system_matrix4x4                  projection_matrix = main_get_projection_matrix();

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    system_matrix4x4_set_from_matrix4x4(_inv_projection,
                                        projection_matrix);
    system_matrix4x4_invert            (_inv_projection);

    entrypoints->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    _fbo_id);
    entrypoints->pGLDrawBuffers    (1, /* n */
                                   &draw_buffer);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_FLYBY,
                            &flyby);
    ogl_flyby_get_property  (flyby,
                             OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                            &_mv);

    ogl_skybox_draw(_skybox,
                    NULL, /* light_sh_data_tbo */
                    _mv,
                    _inv_projection);
}

/* Please see header for specification */
PUBLIC void stage_step_background_deinit(ogl_context context)
{
    system_matrix4x4_release(_inv_projection);
    system_matrix4x4_release(_mv);

    ogl_texture_release(_result_texture);

    ogl_skybox_release (_skybox);
    ogl_texture_release(_skybox_texture);
    gfx_image_release  (_skybox_image);
}

/* Please see header for specification */
PUBLIC ogl_texture stage_step_background_get_background_texture()
{
    return _skybox_texture;
}

/* Please see header for specification */
PUBLIC ogl_texture stage_step_background_get_result_texture()
{
    return _result_texture;
}

/* Please see header for specification */
PUBLIC void stage_step_background_init(ogl_context  context,
                                       ogl_pipeline pipeline,
                                       uint32_t     stage_id)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* Initialize matrix instance */
    _inv_projection = system_matrix4x4_create();
    _mv             = system_matrix4x4_create();

    /* Initialize result texture */
    _result_texture = ogl_texture_create_empty(context,
                                               system_hashed_ansi_string_create("BG result texture") );

    dsa_entrypoints->pGLTextureImage2DEXT   (_result_texture,
                                             GL_TEXTURE_2D,
                                             0,  /* level */
                                             GL_RGBA32F,
                                             main_get_output_resolution()[0],
                                             main_get_output_resolution()[1],
                                             0, /* border */
                                             GL_RGBA,
                                             GL_FLOAT,
                                             NULL); /* pixels */
    dsa_entrypoints->pGLTextureParameteriEXT(_result_texture,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_MIN_FILTER,
                                             GL_LINEAR);

    /* Initialize FBO */
    entrypoints->pGLGenFramebuffers(1, /* n */
                                    &_fbo_id);

    dsa_entrypoints->pGLNamedFramebufferTexture2DEXT(_fbo_id,
                                                     GL_COLOR_ATTACHMENT0,
                                                     GL_TEXTURE_2D,
                                                     _result_texture,
                                                     0);

    /* Initialize skybox data */
    _skybox_image   = gfx_rgbe_load_from_file                       (system_hashed_ansi_string_create("galileo_probe.hdr"),
                                                                     NULL); /* file_unpacker */
    _skybox_texture = ogl_texture_create_from_gfx_image             (context,
                                                                     _skybox_image,
                                                                     system_hashed_ansi_string_create("skybox texture") );
    _skybox         = ogl_skybox_create_spherical_projection_texture(context,
                                                                     _skybox_texture,
                                                                     system_hashed_ansi_string_create("skybox") );

    /* Add ourselves to the pipeline */
    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                                system_hashed_ansi_string_create("Background"),
                                _stage_step_background_execute,
                                NULL); /* step_callback_user_arg */
}
