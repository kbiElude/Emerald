/**
 *
 * Texture Compression tool (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "gfx/gfx_image.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_texture_compression.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_button.h"
#include "ogl/ogl_ui_checkbox.h"
#include "ogl/ogl_ui_dropdown.h"
#include "ogl/ogl_ui_frame.h"
#include "ogl/ogl_ui_label.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_utils.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_resources.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include <sstream>

struct _compression_algorithm
{
    system_hashed_ansi_string extension;
    GLuint                    gl_enum;
    unsigned int              n_compression_algorithm_engine; /* index as per ogl_context_texture_compression indexing */
    unsigned int              n_compression_algorithm_vector; /* index as per compression algorithm vector AFTER sorting */
    system_hashed_ansi_string name;
    GLint                     size_all_mipmaps;
    GLint                     size_base_mipmap;

    _compression_algorithm()
    {
        extension                      = NULL;
        gl_enum                        = GL_NONE;
        name                           = NULL;
        n_compression_algorithm_engine = -1;
        n_compression_algorithm_vector = -1;
        size_all_mipmaps               = 0;
        size_base_mipmap               = 0;
    }
};

/* Default UI control locations. Y components are dynamically adjusted during run-time */
#define UI_CONTROL_Y_SEPARATOR_PX (4)

const float _default_frame_x1y1x2y2                     [] = {0.7f,  0.2f, 0.8f, 0.5f};
const float _default_compressed_filesize_label_x1y1     [] = {0.7f,  0.2f};
const float _default_compression_algorithm_dropdown_x1y1[] = {0.7f,  0.1f};
const float _default_compressed_filename_label_x1y1     [] = {0.7f,  0.6f};
const float _default_decompressed_filesize_label_x1y1   [] = {0.7f,  0.3f};
const float _default_filename_label_x1y1                [] = {0.7f,  0.4f};
const float _default_generate_mipmaps_checkbox_x1y1     [] = {0.7f,  0.7f};
const float _default_previous_button_x1y1               [] = {0.7f,  0.5f};
const float _default_save_button_x1y1                   [] = {0.75f, 0.5f};
const float _default_next_button_x1y1                   [] = {0.8f,  0.5f};

system_resizable_vector         _compression_algorithms                = NULL;
GLuint                          _compression_fbo                       = -1;
_compression_algorithm*         _current_compression_algorithm         = NULL;
ogl_context                     _context                               = NULL;
ogl_context_texture_compression _context_texture_compression           = NULL;
bool                            _drag_mode                             = false;
float                           _drag_mode_start_dx_dy[2]              = {0};
unsigned short                  _drag_mode_start_xy[2]                 = {0};
float                           _dx_dy[2]                              = {0.0f, 0.0f};
system_file_enumerator          _file_enumerator                       = NULL;
uint32_t                        _file_enumerator_n_current_file        = 0;
uint32_t                        _file_enumerator_n_files               = 0;
ogl_shader                      _preview_fs                            = NULL;
ogl_program                     _preview_po                            = NULL;
ogl_program_ub                  _preview_po_ub                         = NULL;
raGL_buffer                     _preview_po_ub_bo                      = NULL;
GLuint                          _preview_po_ub_bo_size                 = 0;
GLint                           _preview_po_uniform_texture            = -1;
GLint                           _preview_po_uniform_x1y1x2y2_ub_offset = -1;
ogl_shader                      _preview_vs                            = NULL;
GLuint                          _sampler_id                            = 0;
float                           _scale                                 = 1.0f;
ogl_text                        _text_renderer                         = NULL;
ogl_texture                     _texture_c                             = NULL;
ogl_texture                     _texture_nc                            = NULL;
ogl_ui                          _ui                                    = NULL;
system_critical_section         _ui_callback_cs                        = NULL;
ogl_ui_control                  _ui_compressed_filename_label          = NULL;
ogl_ui_control                  _ui_compressed_filesize_label          = NULL;
ogl_ui_control                  _ui_compression_algorithm_dropdown     = NULL;
ogl_ui_control                  _ui_decompressed_filename_label        = NULL;
ogl_ui_control                  _ui_decompressed_filesize_label        = NULL;
ogl_ui_control                  _ui_frame                              = NULL;
ogl_ui_control                  _ui_generate_mipmaps_checkbox          = NULL;
ogl_ui_control                  _ui_next_file_button                   = NULL;
ogl_ui_control                  _ui_previous_file_button               = NULL;
ogl_ui_control                  _ui_save_file_button                   = NULL;
GLuint                          _vao_id                                = 0;
ogl_shader                      _whiteline_fs                          = NULL;
ogl_program                     _whiteline_po                          = NULL;
ogl_shader                      _whiteline_vs                          = NULL;
system_window                   _window                                = NULL;
system_event                    _window_closed_event                   = system_event_create(true); /* manual_reset */
int                             _window_size[2]                        = {1280, 720};
float                           _x1y1x2y2[4]                           = {0.0f, 0.0f, 0.0f, 0.0f};

/** Shaders: preview */
const char* preview_fs_code = "#version 430 core\n"
                              "\n"
                              "out     vec4      result;\n"
                              "in      vec2      uv;\n"
                              "uniform sampler2D texture;\n"
                              "\n"
                              "void main()\n"
                              "{\n"
                              "    result = textureLod(texture, uv, 0);\n"
                              "}\n";
const char* preview_vs_code = "#version 430 core\n"
                              "\n"
                              "out vec2 uv;\n"
                              "\n"
                              "uniform data\n"
                              "{\n"
                              "    vec4 x1y1x2y2;\n"
                              "};\n"
                              "\n"
                              "void main()\n"
                              "{\n"
                              "    switch (gl_VertexID)\n"
                              "    {\n"
                              "        case 0: gl_Position = vec4(x1y1x2y2.x, x1y1x2y2.y, 0.0, 1.0); uv = vec2(0.0, 0.0); break;\n"
                              "        case 1: gl_Position = vec4(x1y1x2y2.x, x1y1x2y2.w, 0.0, 1.0); uv = vec2(0.0, 1.0); break;\n"
                              "        case 2: gl_Position = vec4(x1y1x2y2.z, x1y1x2y2.y, 0.0, 1.0); uv = vec2(1.0, 0.0); break;\n"
                              "        case 3: gl_Position = vec4(x1y1x2y2.z, x1y1x2y2.w, 0.0, 1.0); uv = vec2(1.0, 1.0); break;\n"
                              "    }\n"
                              "}\n";

/** Shaders: white line */
const char* whiteline_fs_code = "#version 430 core\n"
                                "\n"
                                "out vec4 result;\n"
                                "\n"
                                "void main()\n"
                                "{\n"
                                "    result = vec4(1.0);\n"
                                "}\n";
const char* whiteline_vs_code = "#version 430 core\n"
                                "\n"
                                "void main()\n"
                                "{\n"
                                "    switch (gl_VertexID)\n"
                                "    {\n"
                                "        case 0: gl_Position = vec4(0.0, -1.0, 0.0, 1.0); break;\n"
                                "        case 1: gl_Position = vec4(0.0,  1.0, 0.0, 1.0); break;\n"
                                "    }\n"
                                "}\n";

/* Forward declarations */
void                      _callback_on_dropdown_switch                (ogl_ui_control            control,
                                                                       int                       callback_id,
                                                                       void*                     callback_subscriber_data,
                                                                       void*                     callback_data);
void                      _callback_on_generate_mipmaps_button_clicked(void*                     fire_proc_user_arg,
                                                                       void*                     event_user_arg);
void                      _callback_on_next_file_button_clicked       (void*                     fire_proc_user_arg,
                                                                       void*                     event_user_arg);
void                      _callback_on_previous_file_button_clicked   (void*                     fire_proc_user_arg,
                                                                       void*                     event_user_arg);
void                      _callback_on_rbm_up                         (void*                     arg);
void                      _callback_on_save_file_button_clicked       (void*                     fire_proc_user_arg,
                                                                       void*                     event_user_arg);
void                      _callback_renderer_deinit                   (ogl_context               context);
void                      _change_algorithm_callback                  (void*                     fire_proc_user_arg,
                                                                       void*                     event_user_arg);
void                      _change_algorithm_renderer_callback         (ogl_context               context,
                                                                       void*                     user_arg);
void                      _change_texture_renderer_callback           (ogl_context               context,
                                                                       void*                     user_arg);
bool                      _compression_algorithm_comparator           (void*                     alg1,
                                                                       void*                     alg2);
bool                      _compress_texture                           (ogl_context               context,
                                                                       uint32_t                  n_compressed_internalformat);
system_hashed_ansi_string _get_compressed_filename                    ();
bool                      _load_texture                               (ogl_context               context,
                                                                       system_hashed_ansi_string file_name);
void                      _move_to_next_texture                       (ogl_context               context,
                                                                       bool                      move_forward);
system_hashed_ansi_string _replace_extension                          (system_hashed_ansi_string original_filename,
                                                                      system_hashed_ansi_string new_extension);
void                      _rendering_handler                          (ogl_context               context,
                                                                       uint32_t                  n_frames_rendered,
                                                                       system_time               frame_time,
                                                                       void*                     renderer);
void                      _rendering_lbm_down_callback_handler        (system_window             window,
                                                                       unsigned short            x,
                                                                       unsigned short            y,
                                                                       system_window_vk_status   new_status,
                                                                       void*);
void                      _rendering_lbm_up_callback_handler          (system_window             window,
                                                                       unsigned short            x,
                                                                       unsigned short            y,
                                                                       system_window_vk_status   new_status,
                                                                       void*);
void                      _rendering_mouse_wheel_callback_handler     (system_window             window,
                                                                       unsigned short            x,
                                                                       unsigned short            y,
                                                                       short                     scroll_delta,
                                                                       system_window_vk_status,
                                                                       void*);
void                      _rendering_move_callback_handler            (system_window           window,
                                                                       unsigned short          x,
                                                                       unsigned short          y,
                                                                       system_window_vk_status new_status,
                                                                       void*);
void                      _save_compressed_texture                    ();
void                      _save_compressed_texture_renderer_callback  (ogl_context               context,
                                                                       void*                     user_arg);
void                      _setup_compression_algorithms               (ogl_context               context);
void                      _setup_ui                                   (ogl_context               context);
void                      _update_ui_controls_location                ();
void                      _update_ui_controls_strings                 ();

void _callback_on_dropdown_switch(ogl_ui_control control,
                                  int            callback_id,
                                  void*          callback_subscriber_data,
                                  void*          callback_data)
{
    ogl_rendering_handler rendering_handler = NULL;

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    ogl_rendering_handler_lock_bound_context(rendering_handler);
    {
        _update_ui_controls_location();
    }
    ogl_rendering_handler_unlock_bound_context(rendering_handler);
}

void _callback_on_generate_mipmaps_button_clicked(void* fire_proc_user_arg,
                                                  void* event_user_arg)
{
    _update_ui_controls_strings();
}

void _callback_on_next_file_button_clicked(void* fire_proc_user_arg,
                                           void* event_user_arg)
{
    if (system_critical_section_try_enter(_ui_callback_cs) )
    {
        ogl_context_request_callback_from_context_thread(_context,
                                                         _change_texture_renderer_callback,
                                                         (void*) +1);

        system_critical_section_leave(_ui_callback_cs);
    }
}

void _callback_on_previous_file_button_clicked(void* fire_proc_user_arg,
                                               void* event_user_arg)
{
    if (system_critical_section_try_enter(_ui_callback_cs) )
    {
        ogl_context_request_callback_from_context_thread(_context,
                                                         _change_texture_renderer_callback,
                                                         (void*) -1);

        system_critical_section_leave(_ui_callback_cs);
    }
}

void _callback_on_rbm_up(void* arg)
{
    system_event_set(_window_closed_event);
}

void _callback_on_save_file_button_clicked(void* fire_proc_user_arg,
                                           void* event_user_arg)
{
    if (system_critical_section_try_enter(_ui_callback_cs) )
    {
        ogl_context_request_callback_from_context_thread(_context,
                                                         _save_compressed_texture_renderer_callback,
                                                         NULL);

        system_critical_section_leave(_ui_callback_cs);
    }
}

void _callback_renderer_deinit(ogl_context context)
{
    if (_preview_fs != NULL)
    {
        ogl_shader_release(_preview_fs);

        _preview_fs = NULL;
    }

    if (_preview_po != NULL)
    {
        ogl_program_release(_preview_po);

        _preview_po = NULL;
    }

    if (_preview_vs != NULL)
    {
        ogl_shader_release(_preview_vs);

        _preview_vs = NULL;
    }

    if (_text_renderer != NULL)
    {
        ogl_text_release(_text_renderer);

        _text_renderer = NULL;
    }

    if (_texture_c != NULL)
    {
        ogl_texture_release(_texture_c);

        _texture_c = NULL;
    }

    if (_texture_nc != NULL)
    {
        ogl_texture_release(_texture_nc);

        _texture_nc = NULL;
    }

    if (_ui != NULL)
    {
        ogl_ui_release(_ui);

        _ui = NULL;
    }

    if (_whiteline_fs != NULL)
    {
        ogl_shader_release(_whiteline_fs);

        _whiteline_fs = NULL;
    }

    if (_whiteline_po != NULL)
    {
        ogl_program_release(_whiteline_po);

        _whiteline_po = NULL;
    }

    if (_whiteline_vs != NULL)
    {
        ogl_shader_release(_whiteline_vs);

        _whiteline_vs = NULL;
    }
}

PRIVATE void _callback_window_closed(system_window window,
                                     void*         unused)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _callback_window_closing(system_window window,
                                      void*         unused)
{
    ogl_context context = NULL;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &context);

    _callback_renderer_deinit(context);
}

void _change_algorithm_callback(void* fire_proc_user_arg,
                                void* event_user_arg)
{
    /* Request a rendering context call-back, since we need to compress the texture data
     * into a new texture compression internalformat.
     *
     * This call will block until the request is executed *which is fine* because the
     * UI dropdown callback is made from a thread pool-owned thread.
     */
    ogl_context_request_callback_from_context_thread(_context,
                                                     _change_algorithm_renderer_callback,
                                                     event_user_arg);
}

void _change_algorithm_renderer_callback(ogl_context context,
                                         void*       user_arg)
{
    _compress_texture(context,
                      (uint32_t) (intptr_t) user_arg);

    _update_ui_controls_strings();
}

void _change_texture_renderer_callback(ogl_context context,
                                       void*       user_arg)
{
    _move_to_next_texture      (context,
                                user_arg != (void*) (-1) );
    _compress_texture          (context,
                                _current_compression_algorithm->n_compression_algorithm_vector);
    _update_ui_controls_strings();
}

bool _compression_algorithm_comparator(void* alg1,
                                       void* alg2)
{
    _compression_algorithm* alg1_ptr = (_compression_algorithm*) alg1;
    _compression_algorithm* alg2_ptr = (_compression_algorithm*) alg2;

    return strcmp(system_hashed_ansi_string_get_buffer(alg1_ptr->name),
                  system_hashed_ansi_string_get_buffer(alg2_ptr->name) ) < 0;
}

bool _compress_texture(ogl_context context,
                       uint32_t    n_compressed_internalformat)
{
    /* NOTE: Do NOT lock rendering handler - this function is always called from
     *       a rendering context.
     */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Retrieve the internalformat properties */
    _compression_algorithm* compression_algorithm = NULL;

    if (_compression_algorithms == NULL)
    {
        _setup_compression_algorithms(context);
    }

    system_resizable_vector_get_element_at(_compression_algorithms,
                                           n_compressed_internalformat,
                                           &compression_algorithm);

    /* Set up the FBO. We need to do this every time the conversion is requested,
     * because the source texture object is re-initialized every time a new texture
     * is loaded in.*/
    entry_points->pGLBindFramebuffer     (GL_READ_FRAMEBUFFER,
                                         _compression_fbo);
    entry_points->pGLFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                                          GL_COLOR_ATTACHMENT0,
                                          GL_TEXTURE_2D,
                                          _texture_nc,
                                          0); /* level */

    if (_texture_c != NULL)
    {
        ogl_texture_release(_texture_c);

        _texture_c = NULL;
    }

    _texture_c = ogl_texture_create_empty(context,
                                          system_hashed_ansi_string_create("Compressed texture") );

    /* Set up compressed mipmap chain */
    unsigned int compressed_size_all_mipmaps = 0;
    unsigned int compressed_size_base_mipmap = 0;
    unsigned int n_mipmaps                   = 0;

    ogl_texture_get_property(_texture_nc,
                             OGL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &n_mipmaps);

    entry_points->pGLBindTexture(GL_TEXTURE_2D,
                                 _texture_c);

    for (unsigned int n_mipmap = 0;
                      n_mipmap < n_mipmaps;
                    ++n_mipmap)
    {
        uint32_t mipmap_height = 0;
        int      mipmap_size   = 0;
        uint32_t mipmap_width  = 0;

        ogl_texture_get_mipmap_property(_texture_nc,
                                        n_mipmap,
                                        OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                       &mipmap_width);
        ogl_texture_get_mipmap_property(_texture_nc,
                                        n_mipmap,
                                        OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                       &mipmap_height);

        entry_points->pGLCopyTexImage2D(GL_TEXTURE_2D,
                                        n_mipmap,
                                        compression_algorithm->gl_enum,
                                        0, /* x */
                                        0, /* y */
                                        mipmap_width,
                                        mipmap_height,
                                        0); /* border */

        entry_points->pGLGetTexLevelParameteriv(GL_TEXTURE_2D,
                                                n_mipmap,
                                                GL_TEXTURE_COMPRESSED_IMAGE_SIZE,
                                               &mipmap_size);

        ASSERT_DEBUG_SYNC(mipmap_size != 0,
                          "Compressed texture mipmap size is reported to be 0");

        ogl_texture_set_mipmap_property(_texture_c,
                                        n_mipmap,
                                        OGL_TEXTURE_MIPMAP_PROPERTY_DATA_SIZE,
                                       &mipmap_size);

        if (n_mipmap == 0)
        {
            compressed_size_base_mipmap = mipmap_size;
        }

        compressed_size_all_mipmaps += mipmap_size;
    }

    compression_algorithm->size_base_mipmap = compressed_size_base_mipmap;
    compression_algorithm->size_all_mipmaps = compressed_size_all_mipmaps;

    /* Mark the compression algorithm descriptor as current */
    _current_compression_algorithm = compression_algorithm;

    return true;
}

system_hashed_ansi_string _get_compressed_filename()
{
    system_hashed_ansi_string result = NULL;

    system_file_enumerator_get_file_property(_file_enumerator,
                                             _file_enumerator_n_current_file,
                                             SYSTEM_FILE_ENUMERATOR_FILE_PROPERTY_FILE_NAME,
                                            &result);

    result = _replace_extension(result,
                                _current_compression_algorithm->extension);

    return result;
}

bool _load_texture(ogl_context               context,
                   system_hashed_ansi_string file_name)
{
    uint32_t  image_height  = 0;
    gfx_image image_nc      = NULL;
    uint32_t  image_width   = 0;
    bool      result        = true;
    float     scaled_y_span = 0.0f;

    /* Try to load the file */
    image_nc = gfx_image_create_from_file(file_name,
                                          file_name,
                                          false); /* use_alternative_file_getter */

    if (image_nc == NULL)
    {
        result = false;

        goto end;
    }

    /* Create a texture out of the image */
    if (_texture_nc != NULL)
    {
        ogl_texture_release(_texture_nc);

        _texture_nc = NULL;
    }

    _texture_nc = ogl_texture_create_from_gfx_image(context,
                                                    image_nc,
                                                    system_hashed_ansi_string_create("Uncompressed texture") );

    if (_texture_nc == NULL)
    {
        result = false;

        goto end;
    }

    /* Center the image on the screen. We assume the image needs to completely fit
     * horizontally within the viewport. */
    gfx_image_get_mipmap_property(image_nc,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,
                                  &image_width);
    gfx_image_get_mipmap_property(image_nc,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
                                 &image_height);

    _x1y1x2y2[0] = 0.0f;
    _x1y1x2y2[1] = 0.0f;
    _x1y1x2y2[2] = float(image_width)  / float(_window_size[0]);
    _x1y1x2y2[3] = float(image_height) / float(_window_size[1]);

    if (_x1y1x2y2[2] > 1.0f)
    {
        float ar = _x1y1x2y2[2];

        _x1y1x2y2[2]  = 1.0f;
        _x1y1x2y2[3] /= ar;
    }

    if (_x1y1x2y2[2] < 0.5f)
    {
        float delta = (0.5f - _x1y1x2y2[2]) * 0.5f;

        _x1y1x2y2[0] += delta;
        _x1y1x2y2[2] += delta;
    }

    if (_x1y1x2y2[3] < 1.0f)
    {
        float delta = (1.0f - _x1y1x2y2[3]) * 0.5f;

        _x1y1x2y2[1] += delta;
        _x1y1x2y2[3] += delta;
    }

    /* Adjust to screen space */
    for (uint32_t n_component = 0;
                  n_component < 4;
                ++n_component)
    {
        _x1y1x2y2[n_component] = _x1y1x2y2[n_component] * 2.0f - 1.0f;
    }
end:
    if (image_nc != NULL)
    {
        gfx_image_release(image_nc);

        image_nc = NULL;
    }

    return result;
}

void _move_to_next_texture(ogl_context context,
                           bool        move_forward)
{
    int      delta             = (move_forward) ? 1 : -1;
    int      n_current_texture = (int) _file_enumerator_n_current_file;
    uint32_t n_switches        = 0;

    while (n_switches != _file_enumerator_n_files)
    {
        system_hashed_ansi_string file_name = NULL;

        /* Iterate to next file */
        if (n_current_texture + delta < 0)
        {
            n_current_texture = _file_enumerator_n_files - 1;
        }
        else
        {
            n_current_texture = (n_current_texture + delta) % _file_enumerator_n_files;
        }

        n_switches++;

        system_file_enumerator_get_file_property(_file_enumerator,
                                                 n_current_texture,
                                                 SYSTEM_FILE_ENUMERATOR_FILE_PROPERTY_FILE_NAME,
                                                &file_name);

        if (_load_texture(context,
                          file_name) )
        {
            _file_enumerator_n_current_file = n_current_texture;

            break;
        }
    }

    if (n_switches != _file_enumerator_n_files)
    {
        ::MessageBoxA(NULL,
                      "Cannot load at least one of the found texture files!",
                      "Error",
                      MB_OK | MB_ICONERROR);

        /* Kill the app */
        system_event_set(_window_closed_event);
    }
}

system_hashed_ansi_string _replace_extension(system_hashed_ansi_string original_filename,
                                             system_hashed_ansi_string new_extension)
{
    const char* original_filename_ptr          = system_hashed_ansi_string_get_buffer(original_filename);
    const char* original_filename_last_dot_ptr = strrchr(original_filename_ptr, '.');
    std::string result_filename                = std::string(original_filename_ptr);

    ASSERT_DEBUG_SYNC(original_filename_last_dot_ptr != NULL,
                      "No '.' found in the file name");
    if (original_filename_last_dot_ptr != NULL)
    {
        result_filename  = std::string(original_filename_ptr, original_filename_last_dot_ptr - original_filename_ptr + 1);
        result_filename += system_hashed_ansi_string_get_buffer(new_extension);
    }

    return system_hashed_ansi_string_create(result_filename.c_str() );
}

/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
                        void*       renderer)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    static bool                                               initialized      = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    /* Initialize stuff if it's the first frame */
    if (!initialized)
    {
        /***** White line progarm */
        _whiteline_fs = ogl_shader_create (_context,
                                           RAL_SHADER_TYPE_FRAGMENT,
                                           system_hashed_ansi_string_create("Whiteline FS") );
        _whiteline_po = ogl_program_create(_context,
                                           system_hashed_ansi_string_create("Wniteline PO") );
        _whiteline_vs = ogl_shader_create (_context,
                                           RAL_SHADER_TYPE_VERTEX,
                                           system_hashed_ansi_string_create("Whiteline VS") );

        ogl_shader_set_body(_whiteline_fs,
                            system_hashed_ansi_string_create(whiteline_fs_code) );
        ogl_shader_set_body(_whiteline_vs,
                            system_hashed_ansi_string_create(whiteline_vs_code) );

        ogl_shader_compile(_whiteline_fs);
        ogl_shader_compile(_whiteline_vs);

        ogl_program_attach_shader(_whiteline_po,
                                  _whiteline_fs);
        ogl_program_attach_shader(_whiteline_po,
                                  _whiteline_vs);

        ogl_program_link(_whiteline_po);

        /***** Preview program */
        _preview_fs = ogl_shader_create (_context,
                                         RAL_SHADER_TYPE_FRAGMENT,
                                         system_hashed_ansi_string_create("Preview FS") );
        _preview_po = ogl_program_create(_context,
                                         system_hashed_ansi_string_create("Preview PO"),
                                         OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);
        _preview_vs = ogl_shader_create (_context,
                                         RAL_SHADER_TYPE_VERTEX,
                                         system_hashed_ansi_string_create("Preview VS") );

        ogl_shader_set_body(_preview_fs,
                            system_hashed_ansi_string_create(preview_fs_code) );
        ogl_shader_set_body(_preview_vs,
                            system_hashed_ansi_string_create(preview_vs_code) );

        ogl_shader_compile(_preview_fs);
        ogl_shader_compile(_preview_vs);

        ogl_program_attach_shader(_preview_po,
                                  _preview_fs);
        ogl_program_attach_shader(_preview_po,
                                  _preview_vs);

        ogl_program_link(_preview_po);

        /* Retrieve uniforms */
        const ogl_program_variable* uniform_texture  = NULL;
        const ogl_program_variable* uniform_x1y1x2y2 = NULL;

        ogl_program_get_uniform_by_name(_preview_po,
                                        system_hashed_ansi_string_create("texture"),
                                       &uniform_texture);
        ogl_program_get_uniform_by_name(_preview_po,
                                        system_hashed_ansi_string_create("x1y1x2y2"),
                                       &uniform_x1y1x2y2);

        ASSERT_ALWAYS_SYNC(uniform_texture != NULL,
                           "texture is not recognized");
        ASSERT_ALWAYS_SYNC(uniform_x1y1x2y2 != NULL,
                           "x1y1x2y2 is not recognized");

        _preview_po_uniform_texture            = uniform_texture->location;
        _preview_po_uniform_x1y1x2y2_ub_offset = uniform_x1y1x2y2->block_offset;

        /* Retrieve uniform block data */
        ogl_program_get_uniform_block_by_name(_preview_po,
                                              system_hashed_ansi_string_create("data"),
                                             &_preview_po_ub);

        ogl_program_ub_get_property(_preview_po_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &_preview_po_ub_bo_size);
        ogl_program_ub_get_property(_preview_po_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BO,
                                   &_preview_po_ub_bo);

        /* Instantiate a FBO we will use for conversions */
        entry_points->pGLGenFramebuffers(1,
                                        &_compression_fbo);

        /* Spawn a VAO */
        entry_points->pGLGenVertexArrays(1,
                                         &_vao_id);

        /* Load the first file */
        _file_enumerator_n_current_file = -1;

        _move_to_next_texture(context,
                              true);

        /* Retrieve texture compression handler */
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TEXTURE_COMPRESSION,
                                &_context_texture_compression);

        /* Use the first compression algorithm available to prepare the compressed texture */
        _compress_texture(context,
                          0); /* n_compressed_algorithm */

        /* Set up the texture sampler */
        entry_points->pGLGenSamplers(1,
                                     &_sampler_id);
        entry_points->pGLBindSampler(0, /* unit */
                                     _sampler_id);

        entry_points->pGLSamplerParameteri(_sampler_id,
                                           GL_TEXTURE_MAG_FILTER,
                                           GL_NEAREST);
        entry_points->pGLSamplerParameteri(_sampler_id,
                                           GL_TEXTURE_MIN_FILTER,
                                           GL_NEAREST);

        /* Initialize text renderer */
        const float text_color[4] = {1, 1, 1, 1};
        const float text_scale    = 0.5f;

        _text_renderer = ogl_text_create(system_hashed_ansi_string_create("Text renderer"),
                                         context,
                                         system_resources_get_meiryo_font_table(),
                                         _window_size[0],
                                         _window_size[1]);

        ogl_text_set_text_string_property(_text_renderer,
                                          TEXT_STRING_ID_DEFAULT,
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          text_color);
        ogl_text_set_text_string_property(_text_renderer,
                                          TEXT_STRING_ID_DEFAULT,
                                          OGL_TEXT_STRING_PROPERTY_SCALE,
                                         &text_scale);

        /* Initialize UI renderer */
        _setup_ui(context);

        /* Done initializing */
        initialized = true;
    }

    /* Update uniforms */
    float x1y1x2y2[4] =
    {
        0.5f + _x1y1x2y2[0] + _dx_dy[0],
               _x1y1x2y2[1] + _dx_dy[1],
        0.5f + _x1y1x2y2[2] + _dx_dy[0],
               _x1y1x2y2[3] + _dx_dy[1]
    };

    /* Avoid division by zero problems */
    for (uint32_t n_component = 0;
                  n_component < 4;
                ++n_component)
    {
        if (fabs(x1y1x2y2[n_component] + 0.5f) < 1e-5f)
        {
            x1y1x2y2[n_component] -= 1e-5f;
        }
    }

    float len[4] =
    {
        sqrt((x1y1x2y2[0] + 0.5f) * (x1y1x2y2[0] + 0.5f)),
        sqrt( x1y1x2y2[1]         *  x1y1x2y2[1]),
        sqrt((x1y1x2y2[2] + 0.5f) * (x1y1x2y2[2] + 0.5f)),
        sqrt( x1y1x2y2[3]         * x1y1x2y2[3])
    };

    float norm[4] =
    {
        x1y1x2y2[0] / len[0],
        x1y1x2y2[1] / len[1],
        x1y1x2y2[2] / len[2],
        x1y1x2y2[3] / len[3]
    };
    float norm_scaled[4] =
    {
        -0.5f + norm[0] * len[0] * _scale,
                norm[1] * len[1] * _scale,
        -0.5f + norm[2] * len[2] * _scale,
                norm[3] * len[3] * _scale,
    };

    ogl_program_ub_set_nonarrayed_uniform_value(_preview_po_ub,
                                                _preview_po_uniform_x1y1x2y2_ub_offset,
                                                norm_scaled,
                                                0, /* src_data_scaled */
                                                sizeof(float) * 4);
    ogl_program_ub_sync                        (_preview_po_ub);

    GLuint   preview_po_ub_bo_id           = 0;
    uint32_t preview_po_ub_bo_start_offset = -1;

    raGL_buffer_get_property(_preview_po_ub_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &preview_po_ub_bo_id);
    raGL_buffer_get_property(_preview_po_ub_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &preview_po_ub_bo_start_offset);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     0, /* index */
                                      preview_po_ub_bo_id,
                                      preview_po_ub_bo_start_offset,
                                     _preview_po_ub_bo_size);

    /* Clear the color buffer */
    entry_points->pGLClear(GL_COLOR_BUFFER_BIT);

    /* Issue a draw call for the non-compressed texture version */
    entry_points->pGLScissor(0, /* x */
                             0, /* y */
                             _window_size[0] / 2,
                             _window_size[1]);
    entry_points->pGLEnable (GL_SCISSOR_TEST);

    entry_points->pGLUseProgram         (ogl_program_get_id(_preview_po) );
    entry_points->pGLBindVertexArray    (_vao_id);

    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                             GL_TEXTURE_2D,
                                             _texture_nc);

    entry_points->pGLBindSampler(0, /* unit */
                                 _sampler_id);

    entry_points->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                0,  /* first */
                                4); /* count */

    /* Now for the compressed version */
    entry_points->pGLScissor(_window_size[0] / 2,
                             0,                   /* y */
                             _window_size[0] / 2,
                             _window_size[1]);

    norm_scaled[0] += 1.0f;
    norm_scaled[2] += 1.0f;

    ogl_program_ub_set_nonarrayed_uniform_value(_preview_po_ub,
                                                _preview_po_uniform_x1y1x2y2_ub_offset,
                                                norm_scaled,
                                                0, /* src_data_scaled */
                                                sizeof(float) * 4);
    ogl_program_ub_sync                        (_preview_po_ub);

    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                             GL_TEXTURE_2D,
                                             _texture_c);

    entry_points->pGLDrawArrays(GL_TRIANGLE_STRIP,
                                0,  /* first */
                                4); /* count */

    /* Done using the preview program. Now draw the white line.*/
    entry_points->pGLDisable(GL_SCISSOR_TEST);

    entry_points->pGLUseProgram(ogl_program_get_id(_whiteline_po) );
    entry_points->pGLDrawArrays(GL_LINES,
                                0,  /* first */
                                2); /* count */

    /* Draw the UI */
    ogl_ui_draw(_ui);

    /* Draw the text strings */
    entry_points->pGLBindSampler(0,/* unit */
                                 0);

    ogl_text_draw(context,
                  _text_renderer);
}

void _rendering_lbm_down_callback_handler(system_window           window,
                                          unsigned short          x,
                                          unsigned short          y,
                                          system_window_vk_status new_status,
                                          void*)
{
    _drag_mode             = true;
    _drag_mode_start_xy[0] = x;
    _drag_mode_start_xy[1] = y;
}

void _rendering_mouse_wheel_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             short                   scroll_delta,
                                             system_window_vk_status,
                                             void*)
{
    float wheel_rate = float(scroll_delta) / float(WHEEL_DELTA);

    _scale += wheel_rate * 0.25f;

    if (_scale < 0.25f)
    {
        _scale = 0.25f;
    }

    if (_scale > 10.0f)
    {
        _scale = 10.0f;
    }
}

void _rendering_move_callback_handler(system_window           window,
                                      unsigned short          x,
                                      unsigned short          y,
                                      system_window_vk_status new_status,
                                      void*)
{
    if (_drag_mode)
    {
        /* Note, we double the difference because of the fact the picture is shrinked by half in X 
         * and is adjusted in Y */
        _dx_dy[0] =  float(int(x) - int(_drag_mode_start_xy[0])) * 2.0f / _scale / float(_window_size[0]);
        _dx_dy[1] = -float(int(y) - int(_drag_mode_start_xy[1])) * 2.0f / _scale / float(_window_size[1]);
    }
}

void _rendering_lbm_up_callback_handler(system_window           window,
                                        unsigned short          x,
                                        unsigned short          y,
                                        system_window_vk_status new_status,
                                        void*)
{
    _drag_mode    = false;
    _x1y1x2y2[0] += _dx_dy[0];
    _x1y1x2y2[1] += _dx_dy[1];
    _x1y1x2y2[2] += _dx_dy[0];
    _x1y1x2y2[3] += _dx_dy[1];
    _dx_dy[0]     = 0.0f;
    _dx_dy[1]     = 0.0f;
}

void _save_compressed_texture()
{
    ASSERT_DEBUG_SYNC(_current_compression_algorithm != NULL,
                      "Current compression algorithm is NULL!");

    /* Determine if we should store all mipmaps, or just the base one */
    bool store_all_mipmaps = false;

    ogl_ui_get_control_property(_ui_generate_mipmaps_checkbox,
                                OGL_UI_CONTROL_PROPERTY_CHECKBOX_CHECK_STATUS,
                               &store_all_mipmaps);

    /* Allocate space for the buffer */
    unsigned int   compressed_data_size = (store_all_mipmaps) ? _current_compression_algorithm->size_all_mipmaps
                                                              : _current_compression_algorithm->size_base_mipmap;
    unsigned char* compressed_data      = new unsigned char[compressed_data_size];
    unsigned int   n_mipmaps_to_store   = 0;

    if (store_all_mipmaps)
    {
        ogl_texture_get_property(_texture_c,
                                 OGL_TEXTURE_PROPERTY_N_MIPMAPS,
                                &n_mipmaps_to_store);
    }
    else
    {
        n_mipmaps_to_store = 1;
    }

    ASSERT_DEBUG_SYNC(compressed_data != NULL,
                      "Out of memory");

    if (compressed_data != NULL)
    {
        /* Retrieve the compressed data */
        const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
        unsigned char*                                            traveller_ptr    = compressed_data;
        unsigned int                                              total_data_size  = 0;

        ogl_context_get_property(_context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &dsa_entry_points);

        for (unsigned int n_mipmap = 0;
                          n_mipmap < n_mipmaps_to_store;
                        ++n_mipmap)
        {
            int data_size = 0;

            ogl_texture_get_mipmap_property(_texture_c,
                                            n_mipmap,
                                            OGL_TEXTURE_MIPMAP_PROPERTY_DATA_SIZE,
                                           &data_size);

            dsa_entry_points->pGLGetCompressedTextureImageEXT(_texture_c,
                                                              GL_TEXTURE_2D,
                                                              n_mipmap,
                                                              traveller_ptr);

            total_data_size += data_size;
            traveller_ptr   += data_size;
        }

        ASSERT_DEBUG_SYNC(total_data_size == compressed_data_size,
                          "Potential buffer overflow bug detected");

        /* Store the data in the file */
        system_hashed_ansi_string compressed_filename = _get_compressed_filename                 ();
        system_file_serializer    serializer          = system_file_serializer_create_for_writing(compressed_filename);

        ASSERT_DEBUG_SYNC(serializer != NULL,
                          "Could not create file serializer");
        if (serializer != NULL)
        {

            /* Prepare and store the headers */
            {
                ogl_context_texture_compression_compressed_blob_header header;

                header.n_mipmaps = n_mipmaps_to_store;

                system_file_serializer_write(serializer,
                                             sizeof(header),
                                            &header);
            }

            for (unsigned int n_mipmap = 0;
                              n_mipmap < n_mipmaps_to_store;
                            ++n_mipmap)
            {
                ogl_context_texture_compression_compressed_blob_mipmap_header mipmap_header;
                unsigned int                                                  mipmap_data_size = 0;
                unsigned int                                                  mipmap_depth     = 0;
                unsigned int                                                  mipmap_height    = 0;
                unsigned int                                                  mipmap_width     = 0;

                ogl_texture_get_mipmap_property(_texture_c,
                                                n_mipmap,
                                                OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                               &mipmap_width);
                ogl_texture_get_mipmap_property(_texture_c,
                                                n_mipmap,
                                                OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                               &mipmap_height);
                ogl_texture_get_mipmap_property(_texture_c,
                                                n_mipmap,
                                                OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                               &mipmap_depth);
                ogl_texture_get_mipmap_property(_texture_c,
                                                n_mipmap,
                                                OGL_TEXTURE_MIPMAP_PROPERTY_DATA_SIZE,
                                               &mipmap_data_size);

                ASSERT_ALWAYS_SYNC(mipmap_depth  < (1 << (8 * sizeof(mipmap_header.depth))),
                                   "Unsupported depth");
                ASSERT_ALWAYS_SYNC(mipmap_height < (1 << (8 * sizeof(mipmap_header.height))),
                                   "Unsupported height");
                ASSERT_ALWAYS_SYNC(mipmap_width  < (1 << (8 * sizeof(mipmap_header.width))),
                                   "Unsupported width");

                mipmap_header.data_size = mipmap_data_size;
                mipmap_header.depth     = mipmap_depth;
                mipmap_header.height    = mipmap_height;
                mipmap_header.width     = mipmap_width;

                system_file_serializer_write(serializer,
                                             sizeof(mipmap_header),
                                            &mipmap_header);
            } /* for (all mipmaps) */

            /* Write the actual data */
            system_file_serializer_write(serializer,
                                         compressed_data_size,
                                         compressed_data);

            system_file_serializer_release(serializer);
        } /* if (serializer != NULL) */

        /* Release the data buffer */
        delete [] compressed_data;
        compressed_data = NULL;
    } /* if (compressed_data != NULL) */
}

void _save_compressed_texture_renderer_callback(ogl_context context,
                                                void*       user_arg)
{
    _save_compressed_texture   ();
    _update_ui_controls_strings();
}

void _setup_compression_algorithms(ogl_context context)
{
    unsigned int                    n_compressed_internalformats = 0;
    ogl_context_texture_compression texture_compression          = NULL;

    /* Determine how many compression algorithms are supported */
    ogl_context_get_property                    (context,
                                                 OGL_CONTEXT_PROPERTY_TEXTURE_COMPRESSION,
                                                &texture_compression);
    ogl_context_texture_compression_get_property(texture_compression,
                                                 OGL_CONTEXT_TEXTURE_COMPRESSION_PROPERTY_N_COMPRESSED_INTERNALFORMAT,
                                                &n_compressed_internalformats);

    /* Allocate global vector */
    _compression_algorithms = system_resizable_vector_create(n_compressed_internalformats);

    for (unsigned int n_internalformat = 0;
                      n_internalformat < n_compressed_internalformats;
                    ++n_internalformat)
    {
        _compression_algorithm* new_algorithm = new _compression_algorithm;

        ogl_context_texture_compression_get_algorithm_property(texture_compression,
                                                               n_internalformat,
                                                               OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_FILE_EXTENSION,
                                                              &new_algorithm->extension);
        ogl_context_texture_compression_get_algorithm_property(texture_compression,
                                                               n_internalformat,
                                                               OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_GL_VALUE,
                                                               &new_algorithm->gl_enum);
        ogl_context_texture_compression_get_algorithm_property(texture_compression,
                                                               n_internalformat,
                                                               OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_NAME,
                                                               &new_algorithm->name);

        /* Is this algorithm supported by RAL? If not, throw it away */
        if (raGL_utils_get_ral_texture_format_for_ogl_enum(new_algorithm->gl_enum) != RAL_TEXTURE_FORMAT_UNKNOWN)
        {
            new_algorithm->n_compression_algorithm_engine = n_internalformat;

            /* Store the entry */
            system_resizable_vector_push(_compression_algorithms,
                                         new_algorithm);
        }
        else
        {
            delete new_algorithm;
        }
    } /* for (all compressed internalformats) */

    /* Sort the compression formats by name */
#if 0
    system_resizable_vector_sort(_compression_algorithms,
                                 _compression_algorithm_comparator);
#endif

    /* Assign vector indices */
    for (unsigned int n_internalformat = 0;
                      n_internalformat < n_compressed_internalformats;
                    ++n_internalformat)
    {
        _compression_algorithm* algorithm_ptr = NULL;

        if (system_resizable_vector_get_element_at(_compression_algorithms,
                                                   n_internalformat,
                                                  &algorithm_ptr) )
        {
            algorithm_ptr->n_compression_algorithm_vector = n_internalformat;
        }
    } /* for (all vector entries) */
}

void _setup_ui(ogl_context context)
{
    /* Use default x1y1 locations during set-up process. We will use a dedicated function
     * to position the controls right after we're done adding them */

    /* Set up a global vector with texture compression algorithm details */
    _setup_compression_algorithms(context);

    /* Convert the vector data to C arrays needed for ogl_ui_add_dropdown() call */
    system_hashed_ansi_string* compressed_internalformats   = NULL;
    unsigned int               n_compressed_internalformats = 0;

    system_resizable_vector_get_property(_compression_algorithms,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_compressed_internalformats);

    compressed_internalformats = new system_hashed_ansi_string[n_compressed_internalformats];

    for (unsigned int n_algorithm = 0;
                      n_algorithm < n_compressed_internalformats;
                    ++n_algorithm)
    {
        _compression_algorithm* entry = NULL;

        system_resizable_vector_get_element_at(_compression_algorithms, n_algorithm, &entry);

        compressed_internalformats[n_algorithm] = entry->name;
    }

    /* As user args, we will use simple int indices */
    void** user_args = new void*[n_compressed_internalformats];

    for (unsigned int n_algorithm = 0;
                      n_algorithm < n_compressed_internalformats;
                    ++n_algorithm)
    {
        user_args[n_algorithm] = (void*) (intptr_t) n_algorithm;
    }

    /* Set up the UI */
    _ui = ogl_ui_create(_text_renderer,
                        system_hashed_ansi_string_create("UI renderer") );

    /* UI: compression algorithm dropdown */
    _ui_compression_algorithm_dropdown = ogl_ui_add_dropdown(_ui,
                                                             n_compressed_internalformats,
                                                             compressed_internalformats,
                                                             user_args,
                                                             0,    /* n_selected_entry */
                                                             system_hashed_ansi_string_create("Compression algorithm:"),
                                                             _default_compression_algorithm_dropdown_x1y1,
                                                             _change_algorithm_callback,
                                                             NULL); /* fire_user_arg */

    ogl_ui_register_control_callback(_ui,
                                     _ui_compression_algorithm_dropdown,
                                     OGL_UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,
                                     _callback_on_dropdown_switch,
                                     NULL); /* callback_proc_user_arg */

    /* UI: frame */
    _ui_frame = ogl_ui_add_frame(_ui,
                                 _default_frame_x1y1x2y2);

    /* UI: compressed file name. We will update the actual text string shortly. */
    _ui_compressed_filename_label = ogl_ui_add_label(_ui,
                                                     system_hashed_ansi_string_create(""),
                                                     _default_compressed_filename_label_x1y1);

    /* UI: compression algorithm file size. We will update the actual text string shortly */
    _ui_compressed_filesize_label = ogl_ui_add_label(_ui,
                                                     system_hashed_ansi_string_create(""),
                                                     _default_compressed_filesize_label_x1y1);

    /* UI: decompressed file size. We will update the actual text string shortly */
    _ui_decompressed_filesize_label = ogl_ui_add_label(_ui,
                                                       system_hashed_ansi_string_create(""),
                                                       _default_decompressed_filesize_label_x1y1);

    /* UI: file name. We will update the actual text string shortly */
    _ui_decompressed_filename_label = ogl_ui_add_label(_ui,
                                                       system_hashed_ansi_string_create(""),
                                                       _default_filename_label_x1y1);

    /* UI: generate mipmaps checkbox. */
    _ui_generate_mipmaps_checkbox = ogl_ui_add_checkbox(_ui,
                                                        system_hashed_ansi_string_create("Generate mipmaps"),
                                                        _default_generate_mipmaps_checkbox_x1y1,
                                                        true, /* default_status */
                                                        _callback_on_generate_mipmaps_button_clicked,
                                                        NULL);

    /* UI: previous button */
    _ui_previous_file_button = ogl_ui_add_button(_ui,
                                                 system_hashed_ansi_string_create("Previous file"),
                                                 _default_previous_button_x1y1,
                                                 _callback_on_previous_file_button_clicked,
                                                 NULL); /* fire_user_arg */

    /* UI: save button */
    _ui_save_file_button = ogl_ui_add_button(_ui,
                                             system_hashed_ansi_string_create("Save"),
                                             _default_save_button_x1y1,
                                             _callback_on_save_file_button_clicked,
                                             NULL); /* fire_user_arg */

    /* UI: next button */
    _ui_next_file_button = ogl_ui_add_button(_ui,
                                             system_hashed_ansi_string_create("Next file"),
                                             _default_next_button_x1y1,
                                             _callback_on_next_file_button_clicked,
                                             NULL); /* fire_user_arg */

    /* Update control text strings */
    _update_ui_controls_strings();

    /* Position the controls */
    _update_ui_controls_location();

    /* Release allocated resources */
    delete [] compressed_internalformats;
    compressed_internalformats = NULL;

    delete [] user_args;
    user_args = NULL;
}

/** TODO */
void _update_ui_controls_location()
{
    int window_size[2] = {0};

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    /* The hierarchy is:
     *
     * 1) frame
     * 2) dropdown
     * 3) all the rest.
     *
     * All components in 3) should be positioned relative to the height of
     * that dropdown.
     */
    float dropdown_label_bg_x1y1x2y2[4];
    float dropdown_label_x1y1       [2];
    float dropdown_x1y1x2y2         [4];
    float label_height_ss;
    float label_x1y1       [2];

    ogl_ui_get_control_property(_ui_compression_algorithm_dropdown,
                                OGL_UI_CONTROL_PROPERTY_DROPDOWN_LABEL_BG_X1Y1X2Y2,
                                dropdown_label_bg_x1y1x2y2);
    ogl_ui_get_control_property(_ui_compression_algorithm_dropdown,
                                OGL_UI_CONTROL_PROPERTY_DROPDOWN_LABEL_X1Y1,
                                dropdown_label_x1y1);
    ogl_ui_get_control_property(_ui_compression_algorithm_dropdown,
                                OGL_UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,
                                dropdown_x1y1x2y2);
    ogl_ui_get_control_property(_ui_compressed_filesize_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_TEXT_HEIGHT_SS,
                               &label_height_ss);

    /* Compressed file name */
    label_x1y1[0] = dropdown_label_x1y1[0];
    label_x1y1[1] = (1.0f - dropdown_x1y1x2y2[1]) + label_height_ss + float(UI_CONTROL_Y_SEPARATOR_PX) / float(window_size[1]);

    ogl_ui_set_control_property(_ui_compressed_filename_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_X1Y1,
                                label_x1y1);

    /* Compressed file size */
    label_x1y1[1] += label_height_ss + float(UI_CONTROL_Y_SEPARATOR_PX) / float(window_size[1]);

    ogl_ui_set_control_property(_ui_compressed_filesize_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_X1Y1,
                                label_x1y1);

    /* Decompressed file name */
    label_x1y1[1] += label_height_ss + float(UI_CONTROL_Y_SEPARATOR_PX) / float(window_size[1]);

    ogl_ui_set_control_property(_ui_decompressed_filename_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_X1Y1,
                                label_x1y1);

    /* Decompressed file size */
    label_x1y1[1] += label_height_ss + float(UI_CONTROL_Y_SEPARATOR_PX) / float(window_size[1]);

    ogl_ui_set_control_property(_ui_decompressed_filesize_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_X1Y1,
                                label_x1y1);


    /* Previous button */
    float button_height;
    float button_width;
    float prev_button_x1y1x2y2[4];

    ogl_ui_get_control_property(_ui_previous_file_button,
                                OGL_UI_CONTROL_PROPERTY_BUTTON_HEIGHT_SS,
                               &button_height);
    ogl_ui_get_control_property(_ui_previous_file_button,
                                OGL_UI_CONTROL_PROPERTY_BUTTON_WIDTH_SS,
                               &button_width);

    prev_button_x1y1x2y2[0] = dropdown_x1y1x2y2[0];
    prev_button_x1y1x2y2[1] = label_x1y1[1] + label_height_ss + 2.0f * float(UI_CONTROL_Y_SEPARATOR_PX) / float(window_size[1]);
    prev_button_x1y1x2y2[2] = prev_button_x1y1x2y2[0] + button_width;
    prev_button_x1y1x2y2[3] = prev_button_x1y1x2y2[1] - button_height;

    ogl_ui_set_control_property(_ui_previous_file_button,
                                OGL_UI_CONTROL_PROPERTY_BUTTON_X1Y1X2Y2,
                                prev_button_x1y1x2y2);

    /* Generate mipmaps button */
    label_x1y1[1] = prev_button_x1y1x2y2[1];

    ogl_ui_set_control_property(_ui_generate_mipmaps_checkbox,
                                OGL_UI_CONTROL_PROPERTY_CHECKBOX_X1Y1,
                                label_x1y1);

    /* Next button */
    float next_button_x1y1x2y2[4];

    ogl_ui_get_control_property(_ui_next_file_button,
                                OGL_UI_CONTROL_PROPERTY_BUTTON_WIDTH_SS,
                               &button_width);

    memcpy(next_button_x1y1x2y2,
           prev_button_x1y1x2y2,
           sizeof(float) * 4);

    next_button_x1y1x2y2[0] = dropdown_x1y1x2y2[2]    - button_width;
    next_button_x1y1x2y2[2] = next_button_x1y1x2y2[0] + button_width;

    ogl_ui_set_control_property(_ui_next_file_button,
                                OGL_UI_CONTROL_PROPERTY_BUTTON_X1Y1X2Y2,
                                next_button_x1y1x2y2);

    /* Save button */
    float dx = (dropdown_x1y1x2y2[2] - dropdown_x1y1x2y2[0]) * 0.3f;
    float save_button_x1y1x2y2[4];

    ogl_ui_get_control_property(_ui_save_file_button,
                                OGL_UI_CONTROL_PROPERTY_BUTTON_WIDTH_SS,
                               &button_width);

    memcpy(save_button_x1y1x2y2,
           prev_button_x1y1x2y2,
           sizeof(float) * 4);

    save_button_x1y1x2y2[0] = prev_button_x1y1x2y2[0] + (next_button_x1y1x2y2[0] - prev_button_x1y1x2y2[0]) * 0.5f;
    save_button_x1y1x2y2[2] = save_button_x1y1x2y2[0] + button_width;

    ogl_ui_set_control_property(_ui_save_file_button,
                                OGL_UI_CONTROL_PROPERTY_BUTTON_X1Y1X2Y2,
                                save_button_x1y1x2y2);

    /* Frame */
    float frame_x1y1x2y2[4] =
    {
        dropdown_label_bg_x1y1x2y2 [0],
        1.0f - dropdown_label_x1y1 [1],
        next_button_x1y1x2y2       [2],
        next_button_x1y1x2y2[1]
    };

    ogl_ui_set_control_property(_ui_frame,
                                OGL_UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2,
                                frame_x1y1x2y2);
}

/* TODO */
void _update_ui_controls_strings()
{
    system_hashed_ansi_string compressed_file_size_has;
    std::stringstream         compressed_file_size_sstream;

    /* Determine if the user wants to store all mipmaps */
    bool should_generate_mipmaps = false;

    ogl_ui_get_control_property(_ui_generate_mipmaps_checkbox,
                                OGL_UI_CONTROL_PROPERTY_CHECKBOX_CHECK_STATUS,
                               &should_generate_mipmaps);

    /* Form string for the "compressed file size" label. */
    ASSERT_DEBUG_SYNC(_current_compression_algorithm != NULL,
                      "Current compression algorithm is NULL");

    compressed_file_size_sstream << "Compressed file size: "
                                 << ((should_generate_mipmaps) ? _current_compression_algorithm->size_all_mipmaps
                                                               : _current_compression_algorithm->size_base_mipmap)
                                 << " bytes";
    compressed_file_size_has = system_hashed_ansi_string_create(compressed_file_size_sstream.str().c_str() );

    ogl_ui_set_control_property(_ui_compressed_filesize_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_TEXT,
                               &compressed_file_size_has);

    /* Form string for the "compressed file name" label */
    system_hashed_ansi_string current_file_has;
    system_hashed_ansi_string current_filename_has = _get_compressed_filename();
    std::stringstream         current_filename_sstream;

    current_filename_sstream << "Compressed file name: ["
                             << system_hashed_ansi_string_get_buffer(current_filename_has)
                             << "] ("
                             << (system_file_enumerator_is_file_present(current_filename_has) ? "found" : "NOT found")
                             << ")";
    current_file_has = system_hashed_ansi_string_create(current_filename_sstream.str().c_str() );

    ogl_ui_set_control_property(_ui_compressed_filename_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_TEXT,
                               &current_file_has);

    /* Form string for the "decompressed file size" label. */
    unsigned int              decompressed_file_size = 0;
    system_hashed_ansi_string decompressed_file_size_has;
    std::stringstream         decompressed_file_size_sstream;
    ral_texture_format        texture_format         = RAL_TEXTURE_FORMAT_UNKNOWN;
    unsigned int              texture_size[3]        = {0};

    ogl_texture_get_mipmap_property(_texture_nc,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                    texture_size + 0);
    ogl_texture_get_mipmap_property(_texture_nc,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                    texture_size + 1);
    ogl_texture_get_mipmap_property(_texture_nc,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                    texture_size + 2);

    ogl_texture_get_property(_texture_nc,
                             OGL_TEXTURE_PROPERTY_FORMAT_RAL,
                            &texture_format);

    switch (texture_format)
    {
        case RAL_TEXTURE_FORMAT_RGB8_UNORM:
        case RAL_TEXTURE_FORMAT_SRGB8_UNORM:
        {
            decompressed_file_size = 3 * texture_size[0] * texture_size[1] * texture_size[2];

            break;
        }

        case RAL_TEXTURE_FORMAT_RGBA8_UNORM:
        {
            decompressed_file_size = 4 * texture_size[0] * texture_size[1] * texture_size[2];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized decompressed texture internalformat");
        }
    } /* switch (texture_format) */

    decompressed_file_size_sstream << "Decompressed file size: "
                                   << decompressed_file_size
                                   << " bytes";
    decompressed_file_size_has = system_hashed_ansi_string_create(decompressed_file_size_sstream.str().c_str() );

    ogl_ui_set_control_property(_ui_decompressed_filesize_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_TEXT,
                               &decompressed_file_size_has);

    /* Form string for the "decompressed file name" label */
    current_filename_sstream.str(std::string());

    system_file_enumerator_get_file_property(_file_enumerator,
                                             _file_enumerator_n_current_file,
                                             SYSTEM_FILE_ENUMERATOR_FILE_PROPERTY_FILE_NAME,
                                            &current_file_has);

    current_filename_sstream << "Decompressed file name: ["
                             << system_hashed_ansi_string_get_buffer(current_file_has)
                             << "] ("
                             << (_file_enumerator_n_current_file + 1)
                             << " / "
                             << (_file_enumerator_n_files)
                             << ")";
    current_filename_has = system_hashed_ansi_string_create(current_filename_sstream.str().c_str() );

    ogl_ui_set_control_property(_ui_decompressed_filename_label,
                                OGL_UI_CONTROL_PROPERTY_LABEL_TEXT,
                               &current_filename_has);
}

/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
#else
    int main()
#endif
{
    bool                  context_result           = false;
    ogl_rendering_handler window_rendering_handler = NULL;
    system_screen_mode    window_screen_mode       = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    /* Carry on */
    system_pixel_format window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                               8,  /* color_buffer_green_bits */
                                                               8,  /* color_buffer_blue_bits  */
                                                               0,  /* color_buffer_alpha_bits */
                                                               16, /* depth_buffer_bits       */
                                                               1,  /* n_samples               */
                                                               0); /* stencil_buffer_bits     */

#if 1
    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    _window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                  window_x1y1x2y2,
                                                  system_hashed_ansi_string_create("Test window"),
                                                  false,
                                                  false, /* vsync_enabled */
                                                  true,  /* visible */
                                                  window_pf);
#else
    system_screen_mode_get         (0,
                                   &window_screen_mode);
    system_screen_mode_get_property(window_screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(window_screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    _window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                              window_screen_mode,
                                              false, /* vsync_enabled */
                                              window_pf);
#endif

    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,                 /* desired_fps */
                                                                            _rendering_handler, /* pfn_rendering_callback */
                                                                            NULL);              /* user_arg */

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                               &_context);
    system_window_set_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                        (void*) _rendering_lbm_down_callback_handler,
                                        NULL);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                        (void*) _rendering_lbm_up_callback_handler,
                                        NULL);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                        (void*) _rendering_move_callback_handler,
                                        NULL);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL,
                                        (void*) _rendering_mouse_wheel_callback_handler,
                                        NULL);

    /* Locate all jpg files */
    _file_enumerator = system_file_enumerator_create(system_hashed_ansi_string_create("*.jpg") );

    if (_file_enumerator == NULL)
    {
        ::MessageBoxA(HWND_DESKTOP,
                      "No .jpg files found in the working directory",
                      "Error",
                      MB_OK | MB_ICONERROR);

        goto end;
    }

    system_file_enumerator_get_property(_file_enumerator,
                                        SYSTEM_FILE_ENUMERATOR_PROPERTY_N_FILES,
                                       &_file_enumerator_n_files);

    _ui_callback_cs = system_critical_section_create();

    /* Register for mouse callbacks */
    system_window_add_callback_func(_window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP,
                                    (void*) _callback_on_rbm_up,
                                    NULL);
    system_window_add_callback_func(_window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                    (void*) _callback_window_closed,
                                    NULL);
    system_window_add_callback_func(_window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                    (void*) _callback_window_closing,
                                    NULL);
    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

end:
    system_window_close (_window);
    system_event_release(_window_closed_event);

    if (_compression_algorithms != NULL)
    {
        _compression_algorithm* entry_ptr = NULL;

        while (system_resizable_vector_pop(_compression_algorithms, &entry_ptr) )
        {
            delete entry_ptr;

            entry_ptr = NULL;
        }

        system_resizable_vector_release(_compression_algorithms);
        _compression_algorithms = NULL;
    } /* if (_compression_algorithms != NULL) */

    if (_ui_callback_cs != NULL)
    {
        system_critical_section_release(_ui_callback_cs);

        _ui_callback_cs = NULL;
    }

    /* Avoid leaking system resources */
    main_force_deinit();

    return 0;
}