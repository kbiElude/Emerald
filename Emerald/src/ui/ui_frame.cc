/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_frame.h"
#include "ui/ui_shared.h"

/** Internal types */
typedef struct
{
    ral_context context;
    ral_program program;
    bool        visible;
    float       x1y1x2y2[4];

    ral_program_block_buffer program_ub;
    GLuint                   program_ub_bo_size;
    GLint                    program_x1y1x2y2_ub_offset;

    /* Cached func ptrs */
    PFNGLBINDBUFFERRANGEPROC     pGLBindBufferRange;
    PFNGLBLENDEQUATIONPROC       pGLBlendEquation;
    PFNGLBLENDFUNCPROC           pGLBlendFunc;
    PFNGLDISABLEPROC             pGLDisable;
    PFNGLDRAWARRAYSPROC          pGLDrawArrays;
    PFNGLENABLEPROC              pGLEnable;
    PFNGLUNIFORMBLOCKBINDINGPROC pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC          pGLUseProgram;
} _ui_frame;

/** Internal variables */
static const char* ui_frame_fragment_shader_body =
    "#version 430 core\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    result = vec4(0.1, 0.1, 0.1f, 0.7);\n"
    "}\n";

static system_hashed_ansi_string ui_frame_program_name = system_hashed_ansi_string_create("UI Frame");

/** TODO */
PRIVATE void _ui_frame_init_program(ui         ui_instance,
                                    _ui_frame* frame_ptr)
{
    /* Create all objects */
    ral_context context = ui_get_context(ui_instance);
    ral_shader  fs      = nullptr;
    ral_shader  vs      = nullptr;

    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create("UI frame fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create("UI frame vertex shader"),
        RAL_SHADER_TYPE_VERTEX
    };

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI frame program")
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader     result_shaders[n_shader_create_info_items];

    if (!ral_context_create_shaders(frame_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    if (!ral_context_create_programs(frame_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &frame_ptr->program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed");
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    /* Set up shaders */
    const system_hashed_ansi_string fs_body = system_hashed_ansi_string_create(ui_frame_fragment_shader_body);
    const system_hashed_ansi_string vs_body = system_hashed_ansi_string_create(ui_general_vertex_shader_body);

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body);

    /* Set up program object */
    if (!ral_program_attach_shader(frame_ptr->program,
                                   fs,
                                   true /* async */) ||
        !ral_program_attach_shader(frame_ptr->program,
                                   vs,
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");
    }

    /* Register the prgoram with UI so following frame instances will reuse the program */
    ui_register_program(ui_instance,
                        ui_frame_program_name,
                        frame_ptr->program);

    /* Release shaders we will no longer need */
    const ral_shader shaders_to_release[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(frame_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               (const void**) shaders_to_release);
}

/** Please see header for specification */
PUBLIC void ui_frame_deinit(void* internal_instance)
{
    _ui_frame* ui_frame_ptr = reinterpret_cast<_ui_frame*>(internal_instance);

    ral_context_delete_objects(ui_frame_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &ui_frame_ptr->program);

    if (ui_frame_ptr->program_ub != nullptr)
    {
        ral_program_block_buffer_release(ui_frame_ptr->program_ub);

        ui_frame_ptr->program_ub = nullptr;
    }

    delete ui_frame_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ui_frame_draw(void* internal_instance)
{
    _ui_frame* frame_ptr = reinterpret_cast<_ui_frame*>(internal_instance);

    /* Update uniforms */
    raGL_program program_raGL    = ral_context_get_program_gl(frame_ptr->context,
                                                              frame_ptr->program);
    GLuint       program_raGL_id = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    ral_program_block_buffer_set_nonarrayed_variable_value(frame_ptr->program_ub,
                                                           frame_ptr->program_x1y1x2y2_ub_offset,
                                                           frame_ptr->x1y1x2y2,
                                                           sizeof(float) * 4);

    /* Draw */
    frame_ptr->pGLBlendEquation(GL_FUNC_ADD);
    frame_ptr->pGLBlendFunc    (GL_SRC_ALPHA,
                                GL_ONE_MINUS_SRC_ALPHA);
    frame_ptr->pGLEnable       (GL_BLEND);
    {
        GLuint      program_ub_bo_id           = 0;
        raGL_buffer program_ub_bo_raGL         = nullptr;
        ral_buffer  program_ub_bo_ral          = nullptr;
        uint32_t    program_ub_bo_start_offset = -1;

        ral_program_block_buffer_get_property(frame_ptr->program_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_ub_bo_ral);

        program_ub_bo_raGL = ral_context_get_buffer_gl(frame_ptr->context,
                                                       program_ub_bo_ral);

        raGL_buffer_get_property(program_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &program_ub_bo_id);
        raGL_buffer_get_property(program_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &program_ub_bo_start_offset);

        frame_ptr->pGLUseProgram     (program_raGL_id);
        frame_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                      0, /* index */
                                      program_ub_bo_id,
                                      program_ub_bo_start_offset,
                                      frame_ptr->program_ub_bo_size);

        ral_program_block_buffer_sync(frame_ptr->program_ub);

        frame_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                 0,  /* first */
                                 4); /* count */
    }
    frame_ptr->pGLDisable(GL_BLEND);
}

/** Please see header for specification */
PUBLIC void ui_frame_get_property(const void*         frame,
                                  ui_control_property property,
                                  void*               out_result)
{
    const _ui_frame* frame_ptr = reinterpret_cast<const _ui_frame*>(frame);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED:
        {
            *(float*) out_result = frame_ptr->x1y1x2y2[3] - frame_ptr->x1y1x2y2[1];

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = frame_ptr->visible;

            break;
        }

        case UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2:
        {
            ((float*) out_result)[0] =        frame_ptr->x1y1x2y2[0];
            ((float*) out_result)[1] = 1.0f - frame_ptr->x1y1x2y2[1];
            ((float*) out_result)[2] =        frame_ptr->x1y1x2y2[2];
            ((float*) out_result)[3] = 1.0f - frame_ptr->x1y1x2y2[3];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC void* ui_frame_init(ui           instance,
                           const float* x1y1x2y2)
{
    _ui_frame* new_frame_ptr = new (std::nothrow) _ui_frame;

    ASSERT_ALWAYS_SYNC(new_frame_ptr != nullptr,
                       "Out of memory");

    if (new_frame_ptr != nullptr)
    {
        /* Initialize fields */
        memset(new_frame_ptr,
               0,
               sizeof(_ui_frame) );

        new_frame_ptr->context     = ui_get_context(instance);
        new_frame_ptr->visible     = true;
        new_frame_ptr->x1y1x2y2[0] = x1y1x2y2[0];
        new_frame_ptr->x1y1x2y2[1] = 1.0f - x1y1x2y2[1];
        new_frame_ptr->x1y1x2y2[2] = x1y1x2y2[2];
        new_frame_ptr->x1y1x2y2[3] = 1.0f - x1y1x2y2[3];

        /* Cache GL func pointers */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ral_context_get_property(new_frame_ptr->context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points_ptr = nullptr;

            ogl_context_get_property(ral_context_get_gl_context(new_frame_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points_ptr);

            new_frame_ptr->pGLBindBufferRange     = entry_points_ptr->pGLBindBufferRange;
            new_frame_ptr->pGLBlendEquation       = entry_points_ptr->pGLBlendEquation;
            new_frame_ptr->pGLBlendFunc           = entry_points_ptr->pGLBlendFunc;
            new_frame_ptr->pGLDisable             = entry_points_ptr->pGLDisable;
            new_frame_ptr->pGLDrawArrays          = entry_points_ptr->pGLDrawArrays;
            new_frame_ptr->pGLEnable              = entry_points_ptr->pGLEnable;
            new_frame_ptr->pGLUniformBlockBinding = entry_points_ptr->pGLUniformBlockBinding;
            new_frame_ptr->pGLUseProgram          = entry_points_ptr->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized backend type");

            const ogl_context_gl_entrypoints* entry_points_ptr = nullptr;

            ogl_context_get_property(ral_context_get_gl_context(new_frame_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points_ptr);

            new_frame_ptr->pGLBindBufferRange     = entry_points_ptr->pGLBindBufferRange;
            new_frame_ptr->pGLBlendEquation       = entry_points_ptr->pGLBlendEquation;
            new_frame_ptr->pGLBlendFunc           = entry_points_ptr->pGLBlendFunc;
            new_frame_ptr->pGLDisable             = entry_points_ptr->pGLDisable;
            new_frame_ptr->pGLDrawArrays          = entry_points_ptr->pGLDrawArrays;
            new_frame_ptr->pGLEnable              = entry_points_ptr->pGLEnable;
            new_frame_ptr->pGLUniformBlockBinding = entry_points_ptr->pGLUniformBlockBinding;
            new_frame_ptr->pGLUseProgram          = entry_points_ptr->pGLUseProgram;
        }

        /* Retrieve the rendering program */
        raGL_program program_raGL    = nullptr;
        GLuint       program_raGL_id = 0;

        new_frame_ptr->program = ui_get_registered_program(instance,
                                                           ui_frame_program_name);

        if (new_frame_ptr->program == nullptr)
        {
            _ui_frame_init_program(instance,
                                   new_frame_ptr);

            ASSERT_DEBUG_SYNC(new_frame_ptr->program != nullptr,
                              "Could not initialize frame UI program");
        }

        program_raGL = ral_context_get_program_gl(new_frame_ptr->context,
                                                  new_frame_ptr->program);

        raGL_program_get_property(program_raGL,
                                  RAGL_PROGRAM_PROPERTY_ID,
                                 &program_raGL_id);

        /* Retrieve the uniform block properties */
        ral_buffer program_ub_bo_ral = nullptr;

        new_frame_ptr->program_ub = ral_program_block_buffer_create(new_frame_ptr->context,
                                                                    new_frame_ptr->program,
                                                                    system_hashed_ansi_string_create("dataVS") );

        ral_program_block_buffer_get_property(new_frame_ptr->program_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_ub_bo_ral);
        ral_buffer_get_property              (program_ub_bo_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &new_frame_ptr->program_ub_bo_size);

        const uint32_t datavs_indexed_bp = 0;

        raGL_program_set_block_property_by_name(program_raGL,
                                                system_hashed_ansi_string_create("dataVS"),
                                                RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                               &datavs_indexed_bp);

        /* Retrieve the uniforms */
        const ral_program_variable* x1y1x2y2_uniform_ral_ptr = nullptr;

        ral_program_get_block_variable_by_name(new_frame_ptr->program,
                                               system_hashed_ansi_string_create("dataVS"),
                                               system_hashed_ansi_string_create("x1y1x2y2"),
                                              &x1y1x2y2_uniform_ral_ptr);

        new_frame_ptr->program_x1y1x2y2_ub_offset = x1y1x2y2_uniform_ral_ptr->block_offset;
    }

    return reinterpret_cast<void*>(new_frame_ptr);
}

/** Please see header for specification */
PUBLIC void ui_frame_set_property(void*               frame,
                                  ui_control_property property,
                                  const void*         data)
{
    _ui_frame* frame_ptr = reinterpret_cast<_ui_frame*>(frame);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2:
        {
            const float* x1y1x2y2 = (const float*) data;

            memcpy(frame_ptr->x1y1x2y2,
                   x1y1x2y2,
                   sizeof(float) * 4);

            frame_ptr->x1y1x2y2[1] = 1.0f - frame_ptr->x1y1x2y2[1];
            frame_ptr->x1y1x2y2[3] = 1.0f - frame_ptr->x1y1x2y2[3];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}