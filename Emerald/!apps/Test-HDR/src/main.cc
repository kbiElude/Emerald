/**
 *
 * HDR test app (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <crtdbg.h>
#include <stdlib.h>
#include "main.h"
#include "curve/curve_container.h"
#include "curve_editor/curve_editor_general.h"
#include "gfx/gfx_image.h"
#include "gfx/gfx_rgbe.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
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
const ogl_program_uniform_descriptor*          _filmic_customizable_program_a_uniform             = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_b_uniform             = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_c_uniform             = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_d_uniform             = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_e_uniform             = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_exposure_uniform      = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_exposure_bias_uniform = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_f_uniform             = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_w_uniform             = NULL;
const ogl_program_uniform_descriptor*          _filmic_customizable_program_tex_uniform           = NULL;
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
const ogl_program_uniform_descriptor* _filmic_program_exposure_uniform          = NULL;
const ogl_program_uniform_descriptor* _filmic_program_tex_uniform               = NULL;
system_variant                        _float_variant                            = NULL;
shaders_fragment_texture2D_linear     _linear_fp                                = NULL;
ogl_program                           _linear_program                           = NULL;
const ogl_program_uniform_descriptor* _linear_program_exposure_uniform          = NULL;
const ogl_program_uniform_descriptor* _linear_program_tex_uniform               = NULL;
postprocessing_reinhard_tonemap       _postprocessing_reinhard_tonemapper       = NULL;
postprocessing_reinhard_tonemap       _postprocessing_reinhard_tonemapper_crude = NULL;
shaders_fragment_texture2D_reinhardt  _reinhardt_fp                             = NULL;
ogl_program                           _reinhardt_program                        = NULL;
const ogl_program_uniform_descriptor* _reinhardt_program_exposure_uniform       = NULL;
const ogl_program_uniform_descriptor* _reinhardt_program_tex_uniform            = NULL;
ogl_text                              _text_renderer                            = NULL;
GLuint                                _texture_height                           = -1;
ogl_texture                           _texture                                  = NULL;
gfx_image                             _texture_image                            = NULL;
GLuint                                _texture_width                            = -1;
system_window                         _window                                   = NULL;
system_event                          _window_closed_event                      = system_event_create(true, false);
shaders_vertex_fullscreen             _vp                                       = NULL;
GLuint                                _vaa_id                                   = 0;


/** Rendering handler */
void _rendering_handler(ogl_context context, uint32_t n_frames_rendered, system_timeline_time frame_time, void* renderer)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entry_points);

    if (_texture == NULL)
    {
        entry_points->pGLGenVertexArrays(1, &_vaa_id);

        _texture = ogl_texture_create(context, system_hashed_ansi_string_create("Photo") );

        entry_points->pGLBindTexture   (GL_TEXTURE_2D, _texture);
        entry_points->pGLTexImage2D    (GL_TEXTURE_2D, 0,                     GL_RGB32F, _texture_width, _texture_height, 0, GL_RGB, GL_FLOAT, gfx_image_get_data_pointer(_texture_image) );
        entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not upload HDR texture, lol.");
    }

    entry_points->pGLBindVertexArray(_vaa_id);

    /* Read exposure value */
    float alpha_value       = 0.0f;
    float white_level_value = 0.0f;
    float variant_value     = 0.0f;

    if (_active_mapping != MAPPING_REINHARDT_GLOBAL && _active_mapping != MAPPING_REINHARDT_GLOBAL_CRUDE)
    {
        curve_container_get_default_value(_exposure_curve, 0, true, _float_variant);
        system_variant_get_float(_float_variant, &variant_value);
    }
    else
    {
        curve_container_get_default_value(_alpha_curve, 0, true, _float_variant);
        system_variant_get_float(_float_variant, &alpha_value);
        curve_container_get_default_value(_white_level_curve, 0, true, _float_variant);
        system_variant_get_float(_float_variant, &white_level_value);
    }

    switch(_active_mapping)
    {
        case MAPPING_LINEAR:
        {
            entry_points->pGLUseProgram   (ogl_program_get_id(_linear_program) );
            entry_points->pGLActiveTexture(GL_TEXTURE0);
            entry_points->pGLBindTexture  (GL_TEXTURE_2D,                             _texture);
            entry_points->pGLUniform1i    (_linear_program_tex_uniform->location,     0);
            entry_points->pGLUniform1f    (_linear_program_exposure_uniform->location, variant_value);

            break;
        }

        case MAPPING_REINHARDT:
        {
            entry_points->pGLUseProgram   (ogl_program_get_id(_reinhardt_program) );
            entry_points->pGLActiveTexture(GL_TEXTURE0);
            entry_points->pGLBindTexture  (GL_TEXTURE_2D,                                 _texture);
            entry_points->pGLUniform1i    (_reinhardt_program_tex_uniform->location,      0);
            entry_points->pGLUniform1f    (_reinhardt_program_exposure_uniform->location, variant_value);

            break;
        }

        case MAPPING_REINHARDT_GLOBAL:
        {
            postprocessing_reinhard_tonemap_execute(_postprocessing_reinhard_tonemapper, _texture, alpha_value, white_level_value, 0);
            break;
        }

        case MAPPING_REINHARDT_GLOBAL_CRUDE:
        {
            postprocessing_reinhard_tonemap_execute(_postprocessing_reinhard_tonemapper_crude, _texture, alpha_value, white_level_value, 0);
            break;
        }

        case MAPPING_FILMIC:
        {
            entry_points->pGLUseProgram   (ogl_program_get_id(_filmic_program) );
            entry_points->pGLActiveTexture(GL_TEXTURE0);
            entry_points->pGLBindTexture  (GL_TEXTURE_2D,                              _texture);
            entry_points->pGLUniform1i    (_filmic_program_tex_uniform->location,      0);
            entry_points->pGLUniform1f    (_filmic_program_exposure_uniform->location, variant_value);

            break;
        }

        case MAPPING_FILMIC_CUSTOMIZABLE:
        {
            float a, b, c, d, e, exposure_bias, f, w;

            curve_container_get_default_value(_filmic_customizable_exposure_bias_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,                           &exposure_bias);
            curve_container_get_default_value(_filmic_customizable_a_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,               &a);
            curve_container_get_default_value(_filmic_customizable_b_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,               &b);
            curve_container_get_default_value(_filmic_customizable_c_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,               &c);
            curve_container_get_default_value(_filmic_customizable_d_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,               &d);
            curve_container_get_default_value(_filmic_customizable_e_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,               &e);
            curve_container_get_default_value(_filmic_customizable_f_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,               &f);
            curve_container_get_default_value(_filmic_customizable_w_curve, 0, true, _float_variant);
            system_variant_get_float         (_float_variant,               &w);

            entry_points->pGLUseProgram   (ogl_program_get_id(_filmic_customizable_program) );
            entry_points->pGLActiveTexture(GL_TEXTURE0);
            entry_points->pGLBindTexture  (GL_TEXTURE_2D,                                                _texture);
            entry_points->pGLUniform1f    (_filmic_customizable_program_a_uniform->location,             a);
            entry_points->pGLUniform1f    (_filmic_customizable_program_b_uniform->location,             b);
            entry_points->pGLUniform1f    (_filmic_customizable_program_c_uniform->location,             c);
            entry_points->pGLUniform1f    (_filmic_customizable_program_d_uniform->location,             d);
            entry_points->pGLUniform1f    (_filmic_customizable_program_e_uniform->location,             e);
            entry_points->pGLUniform1f    (_filmic_customizable_program_exposure_uniform->location,      variant_value);
            entry_points->pGLUniform1f    (_filmic_customizable_program_exposure_bias_uniform->location, exposure_bias);
            entry_points->pGLUniform1f    (_filmic_customizable_program_f_uniform->location,             f);
            entry_points->pGLUniform1f    (_filmic_customizable_program_w_uniform->location,             w);
            entry_points->pGLUniform1i    (_filmic_customizable_program_tex_uniform->location,           0);

            break;
        }
    }

    entry_points->pGLDrawArrays   (GL_TRIANGLE_FAN, 0, 4);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Rendering error.");

    ogl_text_draw(_context, _text_renderer);
}

void _update_text_renderer()
{
    switch (_active_mapping)
    {
        case MAPPING_FILMIC:                 ogl_text_set(_text_renderer, 0, "Filmic tone mapping");                    break;
        case MAPPING_FILMIC_CUSTOMIZABLE:    ogl_text_set(_text_renderer, 0, "Customizable filmic tone mapping");       break;
        case MAPPING_LINEAR:                 ogl_text_set(_text_renderer, 0, "Linear tone mapping");                    break;
        case MAPPING_REINHARDT:              ogl_text_set(_text_renderer, 0, "Reinhardt tone mapping (simple)");        break;
        case MAPPING_REINHARDT_GLOBAL:       ogl_text_set(_text_renderer, 0, "Reinhardt tone mapping (global)");        break; 
        case MAPPING_REINHARDT_GLOBAL_CRUDE: ogl_text_set(_text_renderer, 0, "Reinhardt tone mapping (global crude)");  break; 
    }
}

bool _rendering_lbm_callback_handler(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void*)
{
    system_event_set(_window_closed_event);

    return true;
}

bool _rendering_rbm_callback_handler(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void*)
{
    ((int&)_active_mapping)++;

    if (_active_mapping >= MAPPING_LAST)
    {
        _active_mapping = MAPPING_FIRST;
    }

    _update_text_renderer();

    return true;
}

/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
{
    _float_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    int window_size    [2] = {640, 480};
    int window_x1y1x2y2[4] = {0};
    
    /* Create curves */
    _filmic_customizable_a_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Shoulder strength"),        SYSTEM_VARIANT_FLOAT, 1);
    _filmic_customizable_b_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Linear strength"),          SYSTEM_VARIANT_FLOAT, 1);
    _filmic_customizable_c_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Linear angle"),             SYSTEM_VARIANT_FLOAT, 1);
    _filmic_customizable_d_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Toe strength"),             SYSTEM_VARIANT_FLOAT, 1);
    _filmic_customizable_e_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Toe numerator"),            SYSTEM_VARIANT_FLOAT, 1);
    _filmic_customizable_exposure_bias_curve = curve_container_create(system_hashed_ansi_string_create("Filmic: Exposure bias"),            SYSTEM_VARIANT_FLOAT, 1);
    _filmic_customizable_f_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Toe denominator"),          SYSTEM_VARIANT_FLOAT, 1);
    _filmic_customizable_w_curve             = curve_container_create(system_hashed_ansi_string_create("Filmic: Linear white point value"), SYSTEM_VARIANT_FLOAT, 1);
    _alpha_curve                             = curve_container_create(system_hashed_ansi_string_create("Auto Reinhardt: Key value"),        SYSTEM_VARIANT_FLOAT, 1);
    _white_level_curve                       = curve_container_create(system_hashed_ansi_string_create("Auto Reinhardt: White level"),      SYSTEM_VARIANT_FLOAT, 1);
    _exposure_curve                          = curve_container_create(system_hashed_ansi_string_create("Exposure"),                         SYSTEM_VARIANT_FLOAT, 1);

    system_variant_set_float         (_float_variant,  1);
    curve_container_set_default_value(_exposure_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,     1.0f);
    curve_container_set_default_value(_alpha_curve,       0, _float_variant);
    system_variant_set_float         (_float_variant,     6.0f);
    curve_container_set_default_value(_white_level_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,               0.22f);
    curve_container_set_default_value(_filmic_customizable_a_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,               0.30f);
    curve_container_set_default_value(_filmic_customizable_b_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,               0.10f);
    curve_container_set_default_value(_filmic_customizable_c_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,               0.20f);
    curve_container_set_default_value(_filmic_customizable_d_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,               0.01f);
    curve_container_set_default_value(_filmic_customizable_e_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,               0.3f);
    curve_container_set_default_value(_filmic_customizable_f_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,               11.2f);
    curve_container_set_default_value(_filmic_customizable_w_curve, 0, _float_variant);

    system_variant_set_float         (_float_variant,                           2.0f);
    curve_container_set_default_value(_filmic_customizable_exposure_bias_curve, 0, _float_variant);

    /* Load HDR image */
    //_texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("nave_o366.hdr") );
    //_texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("dani_belgium_oC65.hdr") );
    //_texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("MtTamWest_o281.hdr") );
    _texture_image = gfx_rgbe_load_from_file(system_hashed_ansi_string_create("bigFogMap_oDAA.hdr") );

    window_size[0]  = gfx_image_get_image_width (_texture_image);
    window_size[1]  = gfx_image_get_image_height(_texture_image);
    _texture_height = window_size[1];
    _texture_width  = window_size[0];

    /* Carry on */
    system_window_get_centered_window_position_for_primary_monitor(window_size, window_x1y1x2y2);
    
    _window = system_window_create_not_fullscreen(window_x1y1x2y2, system_hashed_ansi_string_create("Test window"), false, 0, false, false, true);

    ogl_rendering_handler window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"), 30, _rendering_handler, NULL);

    bool context_result = system_window_get_context(_window, &_context);
    ASSERT_DEBUG_SYNC(context_result, "Could not retrieve OGL context");

    system_window_set_rendering_handler (_window, window_rendering_handler);
    system_window_add_callback_func     (_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,  _rendering_lbm_callback_handler, NULL);
    system_window_add_callback_func     (_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN, _rendering_rbm_callback_handler, NULL);

    ogl_rendering_handler_set_fps_counter_visibility(window_rendering_handler, true);

    /* Create text renderer */
    float text_color[4] = {0, 0, 1, 1};
    int   text_pos  [2] = {0, 60};

    _text_renderer = ogl_text_create(system_hashed_ansi_string_create("Text renderer"), 
                                     _context, 
                                     system_resources_get_meiryo_font_table(),
                                     _texture_width,
                                     _texture_height
                                     );

    ogl_text_add_string  (_text_renderer);
    ogl_text_set_color   (_text_renderer, 0, text_color);
    ogl_text_set_position(_text_renderer, 0, text_pos);

    _update_text_renderer();

    /* Create shared VP */
    _vp = shaders_vertex_fullscreen_create(_context, true, system_hashed_ansi_string_create("Plain VP"));

    /* Create linear texture program */
    _linear_fp      = shaders_fragment_texture2D_linear_create(_context, true, system_hashed_ansi_string_create("Linear FP"));
    _linear_program = ogl_program_create                      (_context,       system_hashed_ansi_string_create("Linear Main"));

    ogl_program_attach_shader(_linear_program, shaders_fragment_texture2D_linear_get_shader(_linear_fp) );
    ogl_program_attach_shader(_linear_program, shaders_vertex_fullscreen_get_shader        (_vp) );
    ogl_program_link         (_linear_program);

    ogl_program_get_uniform_by_name(_linear_program, system_hashed_ansi_string_create("exposure"), &_linear_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_linear_program, system_hashed_ansi_string_create("tex"),      &_linear_program_tex_uniform);

    /* Create Reinhardt texture program */
    _reinhardt_fp      = shaders_fragment_texture2D_reinhardt_create(_context, true, system_hashed_ansi_string_create("Reinhardt FP"));
    _reinhardt_program = ogl_program_create                         (_context,       system_hashed_ansi_string_create("Reinhardt Main"));

    ogl_program_attach_shader(_reinhardt_program, shaders_fragment_texture2D_reinhardt_get_shader(_reinhardt_fp) );
    ogl_program_attach_shader(_reinhardt_program, shaders_vertex_fullscreen_get_shader           (_vp) );
    ogl_program_link         (_reinhardt_program);

    ogl_program_get_uniform_by_name(_reinhardt_program, system_hashed_ansi_string_create("exposure"), &_reinhardt_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_reinhardt_program, system_hashed_ansi_string_create("tex"),      &_reinhardt_program_tex_uniform);

    /* Create global Reinhardt post-processors */
    uint32_t texture_size[2] = {_texture_width, _texture_height};
    
    _postprocessing_reinhard_tonemapper       = postprocessing_reinhard_tonemap_create(_context, system_hashed_ansi_string_create("Reinhard postprocessor"),       false, texture_size);
    _postprocessing_reinhard_tonemapper_crude = postprocessing_reinhard_tonemap_create(_context, system_hashed_ansi_string_create("Reinhard postprocessor crude"), true,  texture_size);
    
    /* Create Filmic texture program */
    _filmic_fp      = shaders_fragment_texture2D_filmic_create(_context, true, system_hashed_ansi_string_create("Filmic FP"));
    _filmic_program = ogl_program_create                      (_context,       system_hashed_ansi_string_create("Filmic Main"));

    ogl_program_attach_shader(_filmic_program, shaders_fragment_texture2D_filmic_get_shader(_filmic_fp) );
    ogl_program_attach_shader(_filmic_program, shaders_vertex_fullscreen_get_shader        (_vp) );
    ogl_program_link         (_filmic_program);

    ogl_program_get_uniform_by_name(_filmic_program, system_hashed_ansi_string_create("exposure"), &_filmic_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_filmic_program, system_hashed_ansi_string_create("tex"),      &_filmic_program_tex_uniform);

    /* Create Filmic Customizable texture program */
    _filmic_customizable_fp      = shaders_fragment_texture2D_filmic_customizable_create(_context, true, system_hashed_ansi_string_create("Filmic Customizable FP"));
    _filmic_customizable_program = ogl_program_create                                   (_context,       system_hashed_ansi_string_create("Filmic Customizable Main"));

    ogl_program_attach_shader(_filmic_customizable_program, shaders_fragment_texture2D_filmic_customizable_get_shader(_filmic_customizable_fp) );
    ogl_program_attach_shader(_filmic_customizable_program, shaders_vertex_fullscreen_get_shader                     (_vp) );
    ogl_program_link         (_filmic_customizable_program);

    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("a"),             &_filmic_customizable_program_a_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("b"),             &_filmic_customizable_program_b_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("c"),             &_filmic_customizable_program_c_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("d"),             &_filmic_customizable_program_d_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("e"),             &_filmic_customizable_program_e_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("exposure"),      &_filmic_customizable_program_exposure_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("exposure_bias"), &_filmic_customizable_program_exposure_bias_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("f"),             &_filmic_customizable_program_f_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("tex"),           &_filmic_customizable_program_tex_uniform);
    ogl_program_get_uniform_by_name(_filmic_customizable_program, system_hashed_ansi_string_create("w"),             &_filmic_customizable_program_w_uniform);

    /* Open curve editor */
    curve_editor_show(_context);

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler, 0);

    system_event_wait_single_infinite(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    curve_editor_hide                                     ();
    curve_container_release                               (_alpha_curve);
    curve_container_release                               (_exposure_curve);
    curve_container_release                               (_filmic_customizable_a_curve);
    curve_container_release                               (_filmic_customizable_b_curve);
    curve_container_release                               (_filmic_customizable_c_curve);
    curve_container_release                               (_filmic_customizable_d_curve);
    curve_container_release                               (_filmic_customizable_e_curve);
    curve_container_release                               (_filmic_customizable_exposure_bias_curve);
    curve_container_release                               (_filmic_customizable_f_curve);
    curve_container_release                               (_filmic_customizable_w_curve);
    curve_container_release                               (_white_level_curve);
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
    system_window_close                                   (_window);
    gfx_image_release                                     (_texture_image);
    system_event_release                                  (_window_closed_event);
    system_variant_release                                (_float_variant);

    main_force_deinit();

    return 0;
}