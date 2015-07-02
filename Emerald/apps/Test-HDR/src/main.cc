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
#include "gfx/gfx_image.h"
#include "gfx/gfx_rgbe.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_texture.h"
#include "postprocessing/postprocessing_reinhard_tonemap.h"
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

ogl_context                                    _context                                           = NULL;
curve_container                                _filmic_customizable_exposure_bias_curve           = NULL;
shaders_fragment_texture2D_filmic_customizable _filmic_customizable_fp                            = NULL;
ogl_program                                    _filmic_customizable_program                       = NULL;
const ogl_program_variable*                    _filmic_customizable_program_a_uniform             = NULL;
const ogl_program_variable*                    _filmic_customizable_program_b_uniform             = NULL;
const ogl_program_variable*                    _filmic_customizable_program_c_uniform             = NULL;
const ogl_program_variable*                    _filmic_customizable_program_d_uniform             = NULL;
const ogl_program_variable*                    _filmic_customizable_program_e_uniform             = NULL;
const ogl_program_variable*                    _filmic_customizable_program_exposure_uniform      = NULL;
const ogl_program_variable*                    _filmic_customizable_program_exposure_bias_uniform = NULL;
const ogl_program_variable*                    _filmic_customizable_program_f_uniform             = NULL;
const ogl_program_variable*                    _filmic_customizable_program_w_uniform             = NULL;
const ogl_program_variable*                    _filmic_customizable_program_tex_uniform           = NULL;
ogl_program_ub                                 _filmic_customizable_program_ub                    = NULL;
GLuint                                         _filmic_customizable_program_ub_bo_id              = 0;
GLuint                                         _filmic_customizable_program_ub_bo_size            = 0;
GLuint                                         _filmic_customizable_program_ub_bo_start_offset    = 0;
curve_container                                _filmic_customizable_a_curve                       = NULL;
curve_container                                _filmic_customizable_b_curve                       = NULL;
curve_container                                _filmic_customizable_c_curve                       = NULL;
curve_container                                _filmic_customizable_d_curve                       = NULL;
curve_container                                _filmic_customizable_e_curve                       = NULL;
curve_container                                _filmic_customizable_f_curve                       = NULL;
curve_container                                _filmic_customizable_w_curve                       = NULL;

_mapping_type                         _active_mapping    = MAPPING_FILMIC_CUSTOMIZABLE;
curve_container                       _alpha_curve       = NULL;
curve_container                       _exposure_curve    = NULL;
curve_container                       _white_level_curve = NULL;

shaders_fragment_texture2D_filmic     _filmic_fp                                = NULL;
ogl_program                           _filmic_program                           = NULL;
const ogl_program_variable*           _filmic_program_exposure_uniform          = NULL;
const ogl_program_variable*           _filmic_program_tex_uniform               = NULL;
ogl_program_ub                        _filmic_program_ub                        = NULL;
GLuint                                _filmic_program_ub_bo_id                  = 0;
GLuint                                _filmic_program_ub_bo_size                = 0;
GLuint                                _filmic_program_ub_bo_start_offset        = 0;
system_variant                        _float_variant                            = NULL;
shaders_fragment_texture2D_linear     _linear_fp                                = NULL;
ogl_program                           _linear_program                           = NULL;
const ogl_program_variable*           _linear_program_exposure_uniform          = NULL;
const ogl_program_variable*           _linear_program_tex_uniform               = NULL;
ogl_program_ub                        _linear_program_ub                        = NULL;
GLuint                                _linear_program_ub_bo_id                  = 0;
GLuint                                _linear_program_ub_bo_size                = 0;
GLuint                                _linear_program_ub_bo_start_offset        = 0;
postprocessing_reinhard_tonemap       _postprocessing_reinhard_tonemapper       = NULL;
postprocessing_reinhard_tonemap       _postprocessing_reinhard_tonemapper_crude = NULL;
shaders_fragment_texture2D_reinhardt  _reinhardt_fp                             = NULL;
ogl_program                           _reinhardt_program                        = NULL;
const ogl_program_variable*           _reinhardt_program_exposure_uniform       = NULL;
const ogl_program_variable*           _reinhardt_program_tex_uniform            = NULL;
ogl_program_ub                        _reinhardt_program_ub                     = NULL;
GLuint                                _reinhardt_program_ub_bo_id               = 0;
GLuint                                _reinhardt_program_ub_bo_size             = 0;
GLuint                                _reinhardt_program_ub_bo_start_offset     = 0;
ogl_text                              _text_renderer                            = NULL;
GLuint                                _texture_height                           = -1;
ogl_texture                           _texture                                  = NULL;
gfx_image                             _texture_image                            = NULL;
GLuint                                _texture_width                            = -1;
system_window                         _window                                   = NULL;
system_event                          _window_closed_event                      = system_event_create(true); /* manual_reset */
shaders_vertex_fullscreen             _vp                                       = NULL;
GLuint                                _vaa_id                                   = 0;


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

void _rendering_handler(ogl_context          context,
                        uint32_t             n_frames_rendered,
                        system_timeline_time frame_time,
                        void*                renderer)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
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
        const void* texture_image_data_ptr = NULL;

        gfx_image_get_mipmap_property(_texture_image,
                                      0, /* n_mipmap */
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
                                     &texture_image_data_ptr);

        entry_points->pGLGenVertexArrays(1,
                                        &_vaa_id);

        _texture = ogl_texture_create_empty(context,
                                            system_hashed_ansi_string_create("Photo") );

        entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                       _texture);
        entry_points->pGLTexImage2D   (GL_TEXTURE_2D,
                                       0,              /* level */
                                       GL_RGB32F,
                                       _texture_width,
                                       _texture_height,
                                       0,              /* border */
                                       GL_RGB,
                                       GL_FLOAT,
                                       texture_image_data_ptr);
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
    switch(_active_mapping)
    {
        case MAPPING_LINEAR:
        {
            const GLuint po_id = ogl_program_get_id(_linear_program);

            entry_points->pGLUseProgram      (po_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              _texture);
            entry_points->pGLProgramUniform1i(po_id,
                                              _linear_program_tex_uniform->location,
                                              0);

            ogl_program_ub_set_nonarrayed_uniform_value( _linear_program_ub,
                                                         _linear_program_exposure_uniform->block_offset,
                                                        &variant_value,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );

            ogl_program_ub_sync(_linear_program_ub);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             _linear_program_ub_bo_id,
                                             _linear_program_ub_bo_start_offset,
                                             _linear_program_ub_bo_size);

            break;
        }

        case MAPPING_REINHARDT:
        {
            const GLuint po_id = ogl_program_get_id(_reinhardt_program);

            entry_points->pGLUseProgram      (po_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              _texture);
            entry_points->pGLProgramUniform1i(po_id,
                                              _reinhardt_program_tex_uniform->location,
                                              0);

            ogl_program_ub_set_nonarrayed_uniform_value( _reinhardt_program_ub,
                                                         _reinhardt_program_exposure_uniform->block_offset,
                                                        &variant_value,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );

            ogl_program_ub_sync(_reinhardt_program_ub);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             _reinhardt_program_ub_bo_id,
                                             _reinhardt_program_ub_bo_start_offset,
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
            const GLuint po_id = ogl_program_get_id(_filmic_program);

            entry_points->pGLUseProgram      (po_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              _texture);

            entry_points->pGLProgramUniform1i(po_id,
                                              _filmic_program_tex_uniform->location,
                                              0);

            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_program_ub,
                                                         _filmic_program_exposure_uniform->block_offset,
                                                        &variant_value,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );

            ogl_program_ub_sync(_filmic_program_ub);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             _filmic_program_ub_bo_id,
                                             _filmic_program_ub_bo_start_offset,
                                             _filmic_program_ub_bo_size);

            break;
        }

        case MAPPING_FILMIC_CUSTOMIZABLE:
        {
            float a, b, c, d, e, exposure_bias, f, w;

            const GLuint po_id = ogl_program_get_id(_filmic_customizable_program);

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

            entry_points->pGLUseProgram      (po_id);
            entry_points->pGLActiveTexture   (GL_TEXTURE0);
            entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                              _texture);

            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_a_uniform->block_offset,
                                                        &a,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_b_uniform->block_offset,
                                                        &b,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_c_uniform->block_offset,
                                                        &c,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_d_uniform->block_offset,
                                                        &d,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_e_uniform->block_offset,
                                                        &e,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_exposure_uniform->block_offset,
                                                        &variant_value,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_exposure_bias_uniform->block_offset,
                                                        &exposure_bias,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_f_uniform->block_offset,
                                                        &f,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );
            ogl_program_ub_set_nonarrayed_uniform_value( _filmic_customizable_program_ub,
                                                         _filmic_customizable_program_w_uniform->block_offset,
                                                        &w,
                                                         0, /* src_data_flags */
                                                         sizeof(float) );

            ogl_program_ub_sync(_filmic_customizable_program_ub);

            entry_points->pGLProgramUniform1i(po_id,
                                              _filmic_customizable_program_tex_uniform->location,
                                              0);

            entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                             0, /* index */
                                             _filmic_customizable_program_ub_bo_id,
                                             _filmic_customizable_program_ub_bo_start_offset,
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

void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

void _window_closing_callback_handler(system_window window)
{
    ogl_program_release                                   (_filmic_program);
    ogl_program_release                                   (_filmic_customizable_program);
    ogl_program_release                                   (_linear_program);
    ogl_program_release                                   (_reinhardt_program);
    ogl_text_release                                      (_text_renderer);
    ogl_texture_release                                   (_texture);
    shaders_fragment_texture2D_filmic_customizable_release(_filmic_customizable_fp);
    shaders_fragment_texture2D_filmic_release             (_filmic_fp);
    shaders_fragment_texture2D_reinhardt_release          (_reinhardt_fp);
    shaders_fragment_texture2D_linear_release             (_linear_fp);
    postprocessing_reinhard_tonemap_release               (_postprocessing_reinhard_tonemapper);
    postprocessing_reinhard_tonemap_release               (_postprocessing_reinhard_tonemapper_crude);
    shaders_vertex_fullscreen_release                     (_vp);
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
    system_pixel_format window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                               8,  /* color_buffer_green_bits */
                                                               8,  /* color_buffer_blue_bits  */
                                                               0,  /* color_buffer_alpha_bits */
                                                               8,  /* depth_buffer_bits       */
                                                               1); /* n_samples               */

    system_window_get_centered_window_position_for_primary_monitor(window_size,
                                                                   window_x1y1x2y2);

    _window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                                   window_x1y1x2y2,
                                                                   system_hashed_ansi_string_create("Test window"),
                                                                   false,
                                                                   false, /* vsync_enabled */
                                                                   true,  /* visible */
                                                                   window_pf);

    ogl_rendering_handler window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                                                  30, /* desired_fps */
                                                                                                  _rendering_handler,
                                                                                                  NULL); /* user_arg */

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    system_window_add_callback_func     (_window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                         (void*) _rendering_lbm_callback_handler,
                                         NULL);
    system_window_add_callback_func     (_window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                         (void*) _rendering_rbm_callback_handler,
                                         NULL);
    system_window_add_callback_func     (_window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                         (void*) _window_closed_callback_handler,
                                         NULL);
    system_window_add_callback_func     (_window,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                         SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                         (void*) _window_closing_callback_handler,
                                         NULL);

    ogl_rendering_handler_set_fps_counter_visibility(window_rendering_handler,
                                                     true);
    /* Create shared VP */
    _vp = shaders_vertex_fullscreen_create(_context,
                                           true, /* export_uv */
                                           system_hashed_ansi_string_create("Plain VP"));

    /* Create linear texture program */
    _linear_fp      = shaders_fragment_texture2D_linear_create(_context,
                                                               true,
                                                               system_hashed_ansi_string_create("Linear FP"));
    _linear_program = ogl_program_create                      (_context,
                                                               system_hashed_ansi_string_create("Linear Main"),
                                                               OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(_linear_program,
                             shaders_fragment_texture2D_linear_get_shader(_linear_fp) );
    ogl_program_attach_shader(_linear_program,
                             shaders_vertex_fullscreen_get_shader(_vp) );
    ogl_program_link         (_linear_program);

    ogl_program_get_uniform_by_name(_linear_program,
                                    system_hashed_ansi_string_create("exposure"),
                                   &_linear_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_linear_program,
                                    system_hashed_ansi_string_create("tex"),
                                   &_linear_program_tex_uniform);

    ogl_program_get_uniform_block_by_name(_linear_program,
                                          system_hashed_ansi_string_create("data"),
                                         &_linear_program_ub);
    ogl_program_ub_get_property          (_linear_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                         &_linear_program_ub_bo_size);
    ogl_program_ub_get_property          (_linear_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                         &_linear_program_ub_bo_id);
    ogl_program_ub_get_property          (_linear_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                         &_linear_program_ub_bo_start_offset);

    /* Create Reinhardt texture program */
    _reinhardt_fp      = shaders_fragment_texture2D_reinhardt_create(_context,
                                                                     true, /* export_uv */
                                                                     system_hashed_ansi_string_create("Reinhardt FP"));
    _reinhardt_program = ogl_program_create                         (_context,
                                                                     system_hashed_ansi_string_create("Reinhardt Main"),
                                                                     OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(_reinhardt_program,
                              shaders_fragment_texture2D_reinhardt_get_shader(_reinhardt_fp) );
    ogl_program_attach_shader(_reinhardt_program,
                              shaders_vertex_fullscreen_get_shader(_vp) );
    ogl_program_link         (_reinhardt_program);

    ogl_program_get_uniform_by_name(_reinhardt_program,
                                    system_hashed_ansi_string_create("exposure"),
                                   &_reinhardt_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_reinhardt_program,
                                    system_hashed_ansi_string_create("tex"),
                                   &_reinhardt_program_tex_uniform);

    ogl_program_get_uniform_block_by_name(_reinhardt_program,
                                          system_hashed_ansi_string_create("data"),
                                         &_reinhardt_program_ub);
    ogl_program_ub_get_property          (_reinhardt_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                         &_reinhardt_program_ub_bo_size);
    ogl_program_ub_get_property          (_reinhardt_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                         &_reinhardt_program_ub_bo_id);
    ogl_program_ub_get_property          (_reinhardt_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                         &_reinhardt_program_ub_bo_start_offset);

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
    _filmic_fp      = shaders_fragment_texture2D_filmic_create(_context,
                                                               true, /* should_revert_y */
                                                               system_hashed_ansi_string_create("Filmic FP"));
    _filmic_program = ogl_program_create                      (_context,
                                                               system_hashed_ansi_string_create("Filmic Main"),
                                                               OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(_filmic_program,
                              shaders_fragment_texture2D_filmic_get_shader(_filmic_fp) );
    ogl_program_attach_shader(_filmic_program,
                              shaders_vertex_fullscreen_get_shader(_vp) );
    ogl_program_link         (_filmic_program);

    ogl_program_get_uniform_by_name(_filmic_program,
                                    system_hashed_ansi_string_create("exposure"),
                                   &_filmic_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_filmic_program,
                                    system_hashed_ansi_string_create("tex"),
                                   &_filmic_program_tex_uniform);

    ogl_program_get_uniform_block_by_name(_filmic_program,
                                          system_hashed_ansi_string_create("data"),
                                         &_filmic_program_ub);
    ogl_program_ub_get_property          (_filmic_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                         &_filmic_program_ub_bo_size);
    ogl_program_ub_get_property          (_filmic_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                         &_filmic_program_ub_bo_id);
    ogl_program_ub_get_property          (_filmic_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                         &_filmic_program_ub_bo_start_offset);

    /* Create Filmic Customizable texture program */
    _filmic_customizable_fp      = shaders_fragment_texture2D_filmic_customizable_create(_context,
                                                                                         true, /* should_revert_y */
                                                                                         system_hashed_ansi_string_create("Filmic Customizable FP"));
    _filmic_customizable_program = ogl_program_create                                   (_context,
                                                                                         system_hashed_ansi_string_create("Filmic Customizable Main"),
                                                                                         OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(_filmic_customizable_program,
                              shaders_fragment_texture2D_filmic_customizable_get_shader(_filmic_customizable_fp) );
    ogl_program_attach_shader(_filmic_customizable_program,
                              shaders_vertex_fullscreen_get_shader(_vp) );
    ogl_program_link         (_filmic_customizable_program);

    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("a"),
                                   &_filmic_customizable_program_a_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("b"),
                                   &_filmic_customizable_program_b_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("c"),
                                   &_filmic_customizable_program_c_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("d"),
                                   &_filmic_customizable_program_d_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("e"),
                                   &_filmic_customizable_program_e_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("exposure"),
                                   &_filmic_customizable_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("exposure_bias"),
                                   &_filmic_customizable_program_exposure_bias_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("f"),
                                   &_filmic_customizable_program_f_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("tex"),
                                   &_filmic_customizable_program_tex_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program,
                                    system_hashed_ansi_string_create("w"),
                                   &_filmic_customizable_program_w_uniform);

    ogl_program_get_uniform_block_by_name(_filmic_customizable_program,
                                          system_hashed_ansi_string_create("data"),
                                         &_filmic_customizable_program_ub);
    ogl_program_ub_get_property          (_filmic_customizable_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                         &_filmic_customizable_program_ub_bo_size);
    ogl_program_ub_get_property          (_filmic_customizable_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                         &_filmic_customizable_program_ub_bo_id);
    ogl_program_ub_get_property          (_filmic_customizable_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                         &_filmic_customizable_program_ub_bo_start_offset);

    /* Open curve editor */
    curve_editor_show(_context);

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    curve_editor_hide      ();
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
    system_window_close    (_window);
    gfx_image_release      (_texture_image);
    system_event_release   (_window_closed_event);
    system_variant_release (_float_variant);

    main_force_deinit();

    return 0;
}