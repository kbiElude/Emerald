/**
 *
 * HDR test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "curve/curve_container.h"
#include "curve_editor/curve_editor_general.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "gfx/gfx_image.h"
#include "gfx/gfx_rgbe.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "postprocessing/postprocessing_reinhard_tonemap.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_buffer.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_texture.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "shaders/shaders_fragment_texture2D_filmic_customizable.h"
#include "shaders/shaders_fragment_texture2D_filmic.h"
#include "shaders/shaders_fragment_texture2D_linear.h"
#include "shaders/shaders_fragment_texture2D_plain.h"
#include "shaders/shaders_fragment_texture2D_reinhard.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "system/system_variant.h"

typedef enum
{

    MAPPING_LINEAR,
    MAPPING_REINHARDT,
    MAPPING_REINHARDT_GLOBAL,
    MAPPING_REINHARDT_GLOBAL_CRUDE,
    MAPPING_FILMIC,
    MAPPING_FILMIC_CUSTOMIZABLE,

    MAPPING_LAST,
    MAPPING_FIRST = MAPPING_LINEAR
} _mapping_type;

ral_context                                    _context                                                   = NULL;
curve_container                                _filmic_customizable_exposure_bias_curve                   = NULL;
shaders_fragment_texture2D_filmic_customizable _filmic_customizable_fp                                    = NULL;
ral_program                                    _filmic_customizable_program                               = NULL;
const ral_program_variable*                    _filmic_customizable_program_a_uniform_ral_ptr             = NULL;
const ral_program_variable*                    _filmic_customizable_program_b_uniform_ral_ptr             = NULL;
const ral_program_variable*                    _filmic_customizable_program_c_uniform_ral_ptr             = NULL;
const ral_program_variable*                    _filmic_customizable_program_d_uniform_ral_ptr             = NULL;
const ral_program_variable*                    _filmic_customizable_program_e_uniform_ral_ptr             = NULL;
const ral_program_variable*                    _filmic_customizable_program_exposure_uniform_ral_ptr      = NULL;
const ral_program_variable*                    _filmic_customizable_program_exposure_bias_uniform_ral_ptr = NULL;
const ral_program_variable*                    _filmic_customizable_program_f_uniform_ral_ptr             = NULL;
const ral_program_variable*                    _filmic_customizable_program_w_uniform_ral_ptr             = NULL;
const _raGL_program_variable*                  _filmic_customizable_program_tex_uniform_raGL_ptr          = NULL;
ral_program_block_buffer                       _filmic_customizable_program_ub                            = NULL;
uint32_t                                       _filmic_customizable_program_ub_bo_size                    = 0;
curve_container                                _filmic_customizable_a_curve                               = NULL;
curve_container                                _filmic_customizable_b_curve                               = NULL;
curve_container                                _filmic_customizable_c_curve                               = NULL;
curve_container                                _filmic_customizable_d_curve                               = NULL;
curve_container                                _filmic_customizable_e_curve                               = NULL;
curve_container                                _filmic_customizable_f_curve                               = NULL;
curve_container                                _filmic_customizable_w_curve                               = NULL;

_mapping_type                         _active_mapping    = MAPPING_FILMIC_CUSTOMIZABLE;
curve_container                       _alpha_curve       = NULL;
curve_container                       _exposure_curve    = NULL;
curve_container                       _white_level_curve = NULL;

shaders_fragment_texture2D_filmic     _filmic_fp                                  = NULL;
ral_program                           _filmic_program                             = NULL;
const ral_program_variable*           _filmic_program_exposure_uniform_ral_ptr    = NULL;
const _raGL_program_variable*         _filmic_program_tex_uniform_raGL_ptr        = NULL;
uint32_t                              _filmic_program_ub_bo_size                  = 0;
ral_program_block_buffer              _filmic_program_ub                          = NULL;
system_variant                        _float_variant                              = NULL;
shaders_fragment_texture2D_linear     _linear_fp                                  = NULL;
ral_program                           _linear_program                             = NULL;
const ral_program_variable*           _linear_program_exposure_uniform_ral_ptr    = NULL;
const _raGL_program_variable*         _linear_program_tex_uniform_raGL_ptr        = NULL;
uint32_t                              _linear_program_ub_bo_size                  = 0;
ral_program_block_buffer              _linear_program_ub                          = NULL;
postprocessing_reinhard_tonemap       _postprocessing_reinhard_tonemapper         = NULL;
postprocessing_reinhard_tonemap       _postprocessing_reinhard_tonemapper_crude   = NULL;
shaders_fragment_texture2D_reinhardt  _reinhardt_fp                               = NULL;
ral_program                           _reinhardt_program                          = NULL;
const ral_program_variable*           _reinhardt_program_exposure_uniform_ral_ptr = NULL;
const _raGL_program_variable*         _reinhardt_program_tex_uniform_raGL_ptr     = NULL;
uint32_t                              _reinhardt_program_ub_bo_size               = 0;
ral_program_block_buffer              _reinhardt_program_ub                       = NULL;
ogl_text                              _text_renderer                              = NULL;
uint32_t                              _texture_height                             = -1;
ral_texture                           _texture                                    = NULL;
gfx_image                             _texture_image                              = NULL;
uint32_t                              _texture_width                              = -1;
demo_window                           _window                                     = NULL;
system_event                          _window_closed_event                        = system_event_create(true); /* manual_reset */
shaders_vertex_fullscreen             _vp                                         = NULL;
GLuint                                _vaa_id                                     = 0;


/** Rendering handler */
void _update_text_renderer()
{
    switch (_active_mapping)
    {
        case MAPPING_FILMIC:
        {
            ogl_text_set(_text_renderer,
                         0,
                         "Filmic tone mapping");

            break;
        }

        case MAPPING_FILMIC_CUSTOMIZABLE:
        {
            ogl_text_set(_text_renderer,
                         0,
                         "Customizable filmic tone mapping");

            break;
        }

        case MAPPING_LINEAR:
        {
            ogl_text_set(_text_renderer,
                         0,
                         "Linear tone mapping");

            break;
        }

        case MAPPING_REINHARDT:
        {
            ogl_text_set(_text_renderer,
                         0,
                         "Reinhardt tone mapping (simple)");

            break;
        }

        case MAPPING_REINHARDT_GLOBAL:
        {
            ogl_text_set(_text_renderer,
                         0,
                         "Reinhardt tone mapping (global)");

            break;
        }

        case MAPPING_REINHARDT_GLOBAL_CRUDE:
        {
            ogl_text_set(_text_renderer,
                         0,
                         "Reinhardt tone mapping (global crude)");

            break;
        }
    }
}

void _rendering_handler(ogl_context context_gl,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
                        void*       renderer)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Create text renderer, if needed */
    if (_text_renderer == NULL)
    {
        const float text_color[4] = {0, 0, 1, 1};
        const int   text_pos  [2] = {0, 60};

        _text_renderer = ogl_text_create(system_hashed_ansi_string_create("Text renderer"),
                                         _context,
                                         system_resources_get_meiryo_font_table(),
                                         _texture_width,
                                         _texture_height
                                         );

        ogl_text_add_string              (_text_renderer);
        ogl_text_set_text_string_property(_text_renderer,
                                          0, /* text_string_id */
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          text_color);
        ogl_text_set_text_string_property(_text_renderer,
                                          0, /* text_string_id */
                                          OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                          text_pos);

        _update_text_renderer();
    }

    if (_texture == NULL)
    {
        const void*  texture_image_data_ptr = NULL;
        raGL_texture texture_raGL           = NULL;
        GLuint       texture_raGL_id        = 0;

        gfx_image_get_mipmap_property(_texture_image,
                                      0, /* n_mipmap */
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
                                     &texture_image_data_ptr);

        entry_points->pGLGenVertexArrays(1,
                                        &_vaa_id);

        ral_context_create_textures_from_gfx_images(_context,
                                                    1, /* n_textures */
                                                    &_texture_image,
                                                    &_texture);

        texture_raGL = ral_context_get_texture_gl(_context,
                                                  _texture);

        raGL_texture_get_property(texture_raGL,
                                  RAGL_TEXTURE_PROPERTY_ID,
                                 &texture_raGL_id);

        entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                       texture_raGL_id);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_WRAP_S,
                                       GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_WRAP_T,
                                       GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_MAG_FILTER,
                                       GL_LINEAR);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_MIN_FILTER,
                                       GL_LINEAR);
    }

    entry_points->pGLBindVertexArray(_vaa_id);

    /* Read exposure value */
    float alpha_value       = 0.0f;
    float white_level_value = 0.0f;
    float variant_value     = 0.0f;

    if (_active_mapping != MAPPING_REINHARDT_GLOBAL &&
        _active_mapping != MAPPING_REINHARDT_GLOBAL_CRUDE)
    {
        curve_container_get_default_value(_exposure_curve,
                                          true,
                                          _float_variant);
        system_variant_get_float         (_float_variant,
                                         &variant_value);
    }
    else
    {
        curve_container_get_default_value(_alpha_curve,
                                          true,
                                          _float_variant);
        system_variant_get_float         (_float_variant,
                                         &alpha_value);
        curve_container_get_default_value(_white_level_curve,
                                          true,
                                          _float_variant);
        system_variant_get_float         (_float_variant,
                                         &white_level_value);
    }

    /* Set up uniforms */
    raGL_texture texture_raGL    = ral_context_get_texture_gl(_context,
                                                              _texture);
    GLuint       texture_raGL_id = 0;

    raGL_texture_get_property(texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &texture_raGL_id);

    switch(_active_mapping)
    {
        case MAPPING_LINEAR:
        {
            raGL_buffer  linear_program_ub_bo_raGL              = NULL;
            GLuint       linear_program_ub_bo_raGL_id           = 0;
            uint32_t     linear_program_ub_bo_raGL_start_offset = -1;
            ral_buffer   linear_program_ub_bo_ral               = NULL;
            uint32_t     linear_program_ub_bo_ral_start_offset  = -1;
            raGL_program po_raGL                                = ral_context_get_program_gl(_context,
                                                                                             _linear_program);
            GLuint       po_raGL_id                             = 0;

            ral_program_block_buffer_get_property(_linear_program_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &linear_program_ub_bo_ral);
            raGL_program_get_property            (po_raGL,
                                                  RAGL_PROGRAM_PROPERTY_ID,
                                                 &po_raGL_id);

            linear_program_ub_bo_raGL = ral_context_get_buffer_gl(_context,
                                                                  linear_program_ub_bo_ral);

            ral_buffer_get_property (linear_program_ub_bo_ral,
                                     RAL_BUFFER_PROPERTY_START_OFFSET,
                                    &linear_program_ub_bo_ral_start_offset);
            raGL_buffer_get_property(linear_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &linear_program_ub_bo_raGL_id);
            raGL_buffer_get_property(linear_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &linear_program_ub_bo_raGL_start_offset);

            entry_points->pGLUseProgram      (po_raGL_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              texture_raGL_id);

            entry_points->pGLProgramUniform1i(po_raGL_id,
                                              _linear_program_tex_uniform_raGL_ptr->location,
                                              0);

            ral_program_block_buffer_set_nonarrayed_variable_value(_linear_program_ub,
                                                                   _linear_program_exposure_uniform_ral_ptr->block_offset,
                                                                  &variant_value,
                                                                   sizeof(float) );

            ral_program_block_buffer_sync(_linear_program_ub);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             linear_program_ub_bo_raGL_id,
                                             linear_program_ub_bo_raGL_start_offset + linear_program_ub_bo_ral_start_offset,
                                             _linear_program_ub_bo_size);

            break;
        }

        case MAPPING_REINHARDT:
        {
            const raGL_program po_raGL                                   = ral_context_get_program_gl(_context,
                                                                                                      _reinhardt_program);
            GLuint             po_raGL_id                                = 0;
            raGL_buffer        reinhardt_program_ub_bo_raGL              = NULL;
            GLuint             reinhardt_program_ub_bo_raGL_id           = 0;
            uint32_t           reinhardt_program_ub_bo_raGL_start_offset = -1;
            ral_buffer         reinhardt_program_ub_bo_ral               = NULL;
            uint32_t           reinhardt_program_ub_bo_ral_start_offset  = -1;

            raGL_program_get_property            (po_raGL,
                                                  RAGL_PROGRAM_PROPERTY_ID,
                                                 &po_raGL_id);
            ral_program_block_buffer_get_property(_reinhardt_program_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &reinhardt_program_ub_bo_ral);

            reinhardt_program_ub_bo_raGL = ral_context_get_buffer_gl(_context,
                                                                     reinhardt_program_ub_bo_ral);

            ral_buffer_get_property (reinhardt_program_ub_bo_ral,
                                     RAL_BUFFER_PROPERTY_START_OFFSET,
                                    &reinhardt_program_ub_bo_ral_start_offset);
            raGL_buffer_get_property(reinhardt_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &reinhardt_program_ub_bo_raGL_id);
            raGL_buffer_get_property(reinhardt_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &reinhardt_program_ub_bo_raGL_start_offset);

            entry_points->pGLUseProgram      (po_raGL_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              texture_raGL_id);

            entry_points->pGLProgramUniform1i(po_raGL_id,
                                              _reinhardt_program_tex_uniform_raGL_ptr->location,
                                              0);

            ral_program_block_buffer_set_nonarrayed_variable_value(_reinhardt_program_ub,
                                                                   _reinhardt_program_exposure_uniform_ral_ptr->block_offset,
                                                                  &variant_value,
                                                                   sizeof(float) );
            ral_program_block_buffer_sync                         (_reinhardt_program_ub);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             reinhardt_program_ub_bo_raGL_id,
                                             reinhardt_program_ub_bo_raGL_start_offset + reinhardt_program_ub_bo_ral_start_offset,
                                             _reinhardt_program_ub_bo_size);

            break;
        }

        case MAPPING_REINHARDT_GLOBAL:
        {
            postprocessing_reinhard_tonemap_execute(_postprocessing_reinhard_tonemapper,
                                                    _texture,
                                                    alpha_value,
                                                    white_level_value,
                                                    0);
            break;
        }

        case MAPPING_REINHARDT_GLOBAL_CRUDE:
        {
            postprocessing_reinhard_tonemap_execute(_postprocessing_reinhard_tonemapper_crude,
                                                    _texture,
                                                    alpha_value,
                                                    white_level_value,
                                                    0);
            break;
        }

        case MAPPING_FILMIC:
        {
            raGL_buffer        filmic_program_ub_bo_raGL              = NULL;
            GLuint             filmic_program_ub_bo_raGL_id           = 0;
            uint32_t           filmic_program_ub_bo_raGL_start_offset = -1;
            ral_buffer         filmic_program_ub_bo_ral               = NULL;
            uint32_t           filmic_program_ub_bo_ral_start_offset  = -1;
            const raGL_program po_raGL                                = ral_context_get_program_gl(_context,
                                                                                                   _filmic_program);
            GLuint             po_raGL_id                             = 0;

            raGL_program_get_property            (po_raGL,
                                                  RAGL_PROGRAM_PROPERTY_ID,
                                                 &po_raGL_id);
            ral_program_block_buffer_get_property(_filmic_program_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &filmic_program_ub_bo_ral);

            filmic_program_ub_bo_raGL = ral_context_get_buffer_gl(_context,
                                                                  filmic_program_ub_bo_ral);

            raGL_buffer_get_property(filmic_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &filmic_program_ub_bo_raGL_id);
            raGL_buffer_get_property(filmic_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &filmic_program_ub_bo_raGL_start_offset);
            ral_buffer_get_property (filmic_program_ub_bo_ral,
                                     RAL_BUFFER_PROPERTY_START_OFFSET,
                                    &filmic_program_ub_bo_ral_start_offset);

            entry_points->pGLUseProgram      (po_raGL_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              texture_raGL_id);

            entry_points->pGLProgramUniform1i(po_raGL_id,
                                              _filmic_program_tex_uniform_raGL_ptr->location,
                                              0);

            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_program_ub,
                                                                   _filmic_program_exposure_uniform_ral_ptr->block_offset,
                                                                  &variant_value,
                                                                   sizeof(float) );
            ral_program_block_buffer_sync                         (_filmic_program_ub);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             filmic_program_ub_bo_raGL_id,
                                             filmic_program_ub_bo_raGL_start_offset + filmic_program_ub_bo_ral_start_offset,
                                             _filmic_program_ub_bo_size);

            break;
        }

        case MAPPING_FILMIC_CUSTOMIZABLE:
        {
            float              a, b, c, d, e, exposure_bias, f, w;
            raGL_buffer        filmic_customizable_program_ub_bo_raGL              = NULL;
            GLuint             filmic_customizable_program_ub_bo_raGL_id           = 0;
            uint32_t           filmic_customizable_program_ub_bo_raGL_start_offset = -1;
            ral_buffer         filmic_customizable_program_ub_bo_ral               = NULL;
            uint32_t           filmic_customizable_program_ub_bo_ral_start_offset  = -1;
            const raGL_program po_raGL                                             = ral_context_get_program_gl(_context,
                                                                                                                _filmic_customizable_program);
            GLuint             po_raGL_id                                          = 0;

            raGL_program_get_property            (po_raGL,
                                                  RAGL_PROGRAM_PROPERTY_ID,
                                                 &po_raGL_id);
            ral_program_block_buffer_get_property(_filmic_customizable_program_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                  &filmic_customizable_program_ub_bo_ral);

            filmic_customizable_program_ub_bo_raGL = ral_context_get_buffer_gl(_context,
                                                                               filmic_customizable_program_ub_bo_ral);

            raGL_buffer_get_property(filmic_customizable_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &filmic_customizable_program_ub_bo_raGL_id);
            raGL_buffer_get_property(filmic_customizable_program_ub_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &filmic_customizable_program_ub_bo_raGL_start_offset);
            ral_buffer_get_property (filmic_customizable_program_ub_bo_ral,
                                     RAL_BUFFER_PROPERTY_START_OFFSET,
                                    &filmic_customizable_program_ub_bo_ral_start_offset);

            curve_container_get_default_value(_filmic_customizable_exposure_bias_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &exposure_bias);
            curve_container_get_default_value(_filmic_customizable_a_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &a);
            curve_container_get_default_value(_filmic_customizable_b_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &b);
            curve_container_get_default_value(_filmic_customizable_c_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &c);
            curve_container_get_default_value(_filmic_customizable_d_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &d);
            curve_container_get_default_value(_filmic_customizable_e_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &e);
            curve_container_get_default_value(_filmic_customizable_f_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &f);
            curve_container_get_default_value(_filmic_customizable_w_curve,
                                              true,
                                              _float_variant);
            system_variant_get_float         (_float_variant,
                                             &w);

            entry_points->pGLUseProgram      (po_raGL_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              texture_raGL_id);

            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_a_uniform_ral_ptr->block_offset,
                                                            &a,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_b_uniform_ral_ptr->block_offset,
                                                            &b,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_c_uniform_ral_ptr->block_offset,
                                                            &c,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_d_uniform_ral_ptr->block_offset,
                                                            &d,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_e_uniform_ral_ptr->block_offset,
                                                            &e,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_exposure_uniform_ral_ptr->block_offset,
                                                            &variant_value,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_exposure_bias_uniform_ral_ptr->block_offset,
                                                            &exposure_bias,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_f_uniform_ral_ptr->block_offset,
                                                            &f,
                                                             sizeof(float) );
            ral_program_block_buffer_set_nonarrayed_variable_value(_filmic_customizable_program_ub,
                                                             _filmic_customizable_program_w_uniform_ral_ptr->block_offset,
                                                            &w,
                                                             sizeof(float) );

            ral_program_block_buffer_sync(_filmic_customizable_program_ub);

            entry_points->pGLProgramUniform1i(po_raGL_id,
                                              _filmic_customizable_program_tex_uniform_raGL_ptr->location,
                                              0);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             filmic_customizable_program_ub_bo_raGL_id,
                                             filmic_customizable_program_ub_bo_raGL_start_offset + filmic_customizable_program_ub_bo_ral_start_offset,
                                             _filmic_customizable_program_ub_bo_size);

            break;
        }
    }

    entry_points->pGLDrawArrays(GL_TRIANGLE_FAN,
                                0,  /* first */
                                4); /* couint */

    ogl_text_draw(_context,
                  _text_renderer);
}

bool _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);

    return true;
}

bool _rendering_rbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    ((int&)_active_mapping)++;

    if (_active_mapping >= MAPPING_LAST)
    {
        _active_mapping = MAPPING_FIRST;
    }

    _update_text_renderer();

    return true;
}

void _window_closed_callback_handler(system_window window,
                                     void*         unused)
{
    system_event_set(_window_closed_event);
}

void _window_closing_callback_handler(system_window window,
                                      void*         unused)
{
    curve_editor_hide();

    const ral_program_block_buffer block_buffers_to_release[] =
    {
        _filmic_program_ub,
        _linear_program_ub,
        _reinhardt_program_ub
    };
    const ral_program programs_to_release[] =
    {
        _filmic_program,
        _filmic_customizable_program,
        _linear_program,
        _reinhardt_program
    };

    const uint32_t n_block_buffers_to_release = sizeof(block_buffers_to_release) / sizeof(block_buffers_to_release[0]);
    const uint32_t n_programs_to_release      = sizeof(programs_to_release)      / sizeof(programs_to_release[0]);

    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               n_programs_to_release,
                               (const void**) programs_to_release);
    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               1, /* n_objects */
                               (const void**) &_texture);

    ogl_text_release                                      (_text_renderer);
    shaders_fragment_texture2D_filmic_customizable_release(_filmic_customizable_fp);
    shaders_fragment_texture2D_filmic_release             (_filmic_fp);
    shaders_fragment_texture2D_reinhardt_release          (_reinhardt_fp);
    shaders_fragment_texture2D_linear_release             (_linear_fp);
    postprocessing_reinhard_tonemap_release               (_postprocessing_reinhard_tonemapper);
    postprocessing_reinhard_tonemap_release               (_postprocessing_reinhard_tonemapper_crude);
    shaders_vertex_fullscreen_release                     (_vp);

    for (uint32_t n_block_buffer = 0;
                  n_block_buffer < n_block_buffers_to_release;
                ++n_block_buffer)
    {
        ral_program_block_buffer_release(block_buffers_to_release[n_block_buffer]);
    }
}

/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    _float_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    int window_size    [2] = {640, 480};
    int window_x1y1x2y2[4] = {0};

    /* Create curves */
    _filmic_customizable_a_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Shoulder strength"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _filmic_customizable_b_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Linear strength"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _filmic_customizable_c_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Linear angle"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _filmic_customizable_d_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Toe strength"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _filmic_customizable_e_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Toe numerator"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _filmic_customizable_exposure_bias_curve = curve_container_create(system_hashed_ansi_string_create("Filmic: Exposure bias"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _filmic_customizable_f_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Toe denominator"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _filmic_customizable_w_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Linear white point value"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _alpha_curve                             = curve_container_create(system_hashed_ansi_string_create("Auto Reinhardt: Key value"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _white_level_curve                       = curve_container_create(system_hashed_ansi_string_create("Auto Reinhardt: White level"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);
    _exposure_curve                          = curve_container_create(system_hashed_ansi_string_create("Exposure"),
                                                                      NULL, /* object_manager_path */
                                                                      SYSTEM_VARIANT_FLOAT);

    system_variant_set_float         (_float_variant,
                                      1);
    curve_container_set_default_value(_exposure_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      1.0f);
    curve_container_set_default_value(_alpha_curve,
                                      _float_variant);
    system_variant_set_float         (_float_variant,
                                      6.0f);
    curve_container_set_default_value(_white_level_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      0.22f);
    curve_container_set_default_value(_filmic_customizable_a_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      0.30f);
    curve_container_set_default_value(_filmic_customizable_b_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      0.10f);
    curve_container_set_default_value(_filmic_customizable_c_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      0.20f);
    curve_container_set_default_value(_filmic_customizable_d_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      0.01f);
    curve_container_set_default_value(_filmic_customizable_e_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      0.3f);
    curve_container_set_default_value(_filmic_customizable_f_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      11.2f);
    curve_container_set_default_value(_filmic_customizable_w_curve,
                                      _float_variant);

    system_variant_set_float         (_float_variant,
                                      2.0f);
    curve_container_set_default_value(_filmic_customizable_exposure_bias_curve,
                                      _float_variant);

    /* Load HDR image */
    _texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("nave_o366.hdr"),
    //_texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("dani_belgium_oC65.hdr"),
    //_texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("MtTamWest_o281.hdr"),
    //_texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("bigFogMap_oDAA.hdr"),
                                             NULL); /* file_unpacker */

    gfx_image_get_mipmap_property(_texture_image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
                                  window_size + 1);
    gfx_image_get_mipmap_property(_texture_image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,
                                  window_size + 0);

    _texture_height = window_size[1];
    _texture_width  = window_size[0];

    /* Carry on */
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _rendering_handler;
    ogl_rendering_handler                   rendering_handler  = NULL;
    system_screen_mode                      screen_mode        = NULL;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("HDR test app");

    _window = demo_app_create_window(window_name,
                                     RAL_BACKEND_TYPE_GL,
                                     false /* use_timeline */);

    demo_window_set_property(_window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_size);

    demo_window_show(_window);

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                  (void*) _rendering_rbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  NULL);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                  (void*) _window_closing_callback_handler,
                                  NULL);

    /* Create shared VP */
    _vp = shaders_vertex_fullscreen_create(_context,
                                           true, /* export_uv */
                                           system_hashed_ansi_string_create("Plain VP"));

    /* Create linear texture program */
    const ral_program_create_info linear_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("Linear Main")
    };

    _linear_fp = shaders_fragment_texture2D_linear_create(_context,
                                                          true,
                                                          system_hashed_ansi_string_create("Linear FP"));

    ral_context_create_programs(_context,
                                1, /* n_create_info_items */
                               &linear_po_create_info,
                               &_linear_program);

    ral_program_attach_shader(_linear_program,
                             shaders_fragment_texture2D_linear_get_shader(_linear_fp) );
    ral_program_attach_shader(_linear_program,
                             shaders_vertex_fullscreen_get_shader(_vp) );

    raGL_program linear_po_raGL = ral_context_get_program_gl(_context,
                                                             _linear_program);

    ral_program_get_block_variable_by_name(_linear_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("exposure"),
                                          &_linear_program_exposure_uniform_ral_ptr);
    raGL_program_get_uniform_by_name      (linear_po_raGL,
                                           system_hashed_ansi_string_create("tex"),
                                          &_linear_program_tex_uniform_raGL_ptr);

    _linear_program_ub = ral_program_block_buffer_create(_context,
                                                         _linear_program,
                                                         system_hashed_ansi_string_create("data") );

    ral_buffer linear_po_ub_ral = NULL;

    ral_program_block_buffer_get_property(_linear_program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &linear_po_ub_ral);
    ral_buffer_get_property              (linear_po_ub_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &_linear_program_ub_bo_size);

    /* Create Reinhardt texture program */
    const ral_program_create_info reinhardt_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("Reinhardt Main")
    };

    _reinhardt_fp = shaders_fragment_texture2D_reinhardt_create(_context,
                                                                true, /* export_uv */
                                                                system_hashed_ansi_string_create("Reinhardt FP"));

    ral_context_create_programs(_context,
                                1, /* n_create_info_items */
                               &reinhardt_po_create_info,
                               &_reinhardt_program);

    ral_program_attach_shader(_reinhardt_program,
                              shaders_fragment_texture2D_reinhardt_get_shader(_reinhardt_fp) );
    ral_program_attach_shader(_reinhardt_program,
                              shaders_vertex_fullscreen_get_shader(_vp) );

    raGL_program reinhardt_po_raGL = ral_context_get_program_gl(_context,
                                                                _reinhardt_program);

    ral_program_get_block_variable_by_name(_reinhardt_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("exposure"),
                                          &_reinhardt_program_exposure_uniform_ral_ptr);
    raGL_program_get_uniform_by_name      (reinhardt_po_raGL,
                                           system_hashed_ansi_string_create("tex"),
                                          &_reinhardt_program_tex_uniform_raGL_ptr);

    _reinhardt_program_ub = ral_program_block_buffer_create(_context,
                                                            _reinhardt_program,
                                                            system_hashed_ansi_string_create("data") );

    ral_buffer reinhardt_program_ub_bo_ral = NULL;

    ral_program_block_buffer_get_property(_reinhardt_program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &reinhardt_program_ub_bo_ral);
    ral_buffer_get_property              (reinhardt_program_ub_bo_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &_reinhardt_program_ub_bo_size);

    /* Create global Reinhardt post-processors */
    uint32_t texture_size[2] =
    {
        _texture_width,
        _texture_height
    };

    _postprocessing_reinhard_tonemapper       = postprocessing_reinhard_tonemap_create(_context,
                                                                                       system_hashed_ansi_string_create("Reinhard postprocessor"),
                                                                                       false, /* use_crude_downsampled_lum_average_calculation */
                                                                                       texture_size);
    _postprocessing_reinhard_tonemapper_crude = postprocessing_reinhard_tonemap_create(_context,
                                                                                       system_hashed_ansi_string_create("Reinhard postprocessor crude"),
                                                                                       true, /* use_crude_downsampled_lum_average_calculation */
                                                                                       texture_size);

    /* Create Filmic texture program */
    const ral_program_create_info filmic_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("Filmic Main")
    };

    _filmic_fp = shaders_fragment_texture2D_filmic_create(_context,
                                                          true, /* should_revert_y */
                                                          system_hashed_ansi_string_create("Filmic FP"));

    ral_context_create_programs(_context,
                                1, /* n_create_info_items */
                               &filmic_po_create_info,
                               &_filmic_program);

    ral_program_attach_shader(_filmic_program,
                              shaders_fragment_texture2D_filmic_get_shader(_filmic_fp) );
    ral_program_attach_shader(_filmic_program,
                              shaders_vertex_fullscreen_get_shader(_vp) );

    raGL_program filmic_po_raGL = ral_context_get_program_gl(_context,
                                                             _filmic_program);

    ral_program_get_block_variable_by_name(_filmic_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("exposure"),
                                          &_filmic_program_exposure_uniform_ral_ptr);
    raGL_program_get_uniform_by_name      (filmic_po_raGL,
                                           system_hashed_ansi_string_create("tex"),
                                          &_filmic_program_tex_uniform_raGL_ptr);

    _filmic_program_ub = ral_program_block_buffer_create(_context,
                                                         _filmic_program,
                                                         system_hashed_ansi_string_create("data") );

    ral_buffer filmic_program_ub_bo_ral = NULL;

    ral_program_block_buffer_get_property(_filmic_program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &filmic_program_ub_bo_ral);
    ral_buffer_get_property              (filmic_program_ub_bo_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &_filmic_program_ub_bo_size);

    /* Create Filmic Customizable texture program */
    const ral_program_create_info filmic_customizable_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("Filmic Customizable Main")
    };

    _filmic_customizable_fp      = shaders_fragment_texture2D_filmic_customizable_create(_context,
                                                                                         true, /* should_revert_y */
                                                                                         system_hashed_ansi_string_create("Filmic Customizable FP"));

    ral_context_create_programs(_context,
                                1, /* n_create_info_items */
                               &filmic_customizable_po_create_info,
                               &_filmic_customizable_program);


    ral_program_attach_shader(_filmic_customizable_program,
                              shaders_fragment_texture2D_filmic_customizable_get_shader(_filmic_customizable_fp) );
    ral_program_attach_shader(_filmic_customizable_program,
                              shaders_vertex_fullscreen_get_shader(_vp) );

    raGL_program filmic_customizable_po_raGL = ral_context_get_program_gl(_context,
                                                                          _filmic_customizable_program);

    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("a"),
                                          &_filmic_customizable_program_a_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("b"),
                                          &_filmic_customizable_program_b_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("c"),
                                          &_filmic_customizable_program_c_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("d"),
                                          &_filmic_customizable_program_d_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("e"),
                                          &_filmic_customizable_program_e_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("exposure"),
                                          &_filmic_customizable_program_exposure_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("exposure_bias"),
                                          &_filmic_customizable_program_exposure_bias_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("f"),
                                          &_filmic_customizable_program_f_uniform_ral_ptr);
    raGL_program_get_uniform_by_name      (filmic_customizable_po_raGL,
                                           system_hashed_ansi_string_create("tex"),
                                          &_filmic_customizable_program_tex_uniform_raGL_ptr);
    ral_program_get_block_variable_by_name(_filmic_customizable_program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("w"),
                                          &_filmic_customizable_program_w_uniform_ral_ptr);

    _filmic_customizable_program_ub = ral_program_block_buffer_create(_context,
                                                                      _filmic_customizable_program,
                                                                      system_hashed_ansi_string_create("data") );

    ral_buffer filmic_customizable_program_ub_bo_ral = NULL;

    ral_program_block_buffer_get_property(_filmic_customizable_program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &filmic_customizable_program_ub_bo_ral);
    ral_buffer_get_property              (filmic_customizable_program_ub_bo_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &_filmic_customizable_program_ub_bo_size);

    /* Open curve editor */
    curve_editor_show(_context);

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* rendering_start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    demo_app_destroy_window(window_name);

    curve_container_release(_alpha_curve);
    curve_container_release(_exposure_curve);
    curve_container_release(_filmic_customizable_a_curve);
    curve_container_release(_filmic_customizable_b_curve);
    curve_container_release(_filmic_customizable_c_curve);
    curve_container_release(_filmic_customizable_d_curve);
    curve_container_release(_filmic_customizable_e_curve);
    curve_container_release(_filmic_customizable_exposure_bias_curve);
    curve_container_release(_filmic_customizable_f_curve);
    curve_container_release(_filmic_customizable_w_curve);
    curve_container_release(_white_level_curve);
    gfx_image_release      (_texture_image);
    system_event_release   (_window_closed_event);
    system_variant_release (_float_variant);

    main_force_deinit();

    return 0;
}