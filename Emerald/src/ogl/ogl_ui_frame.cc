/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_frame.h"
#include "ogl/ogl_ui_shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"

/** Internal types */
typedef struct
{
    ogl_context context;
    ogl_program program;
    bool        visible;
    float       x1y1x2y2[4];

    ogl_program_ub program_ub;
    GLuint         program_ub_bo_id;
    GLuint         program_ub_bo_size;
    GLuint         program_ub_bo_start_offset;
    GLint          program_x1y1x2y2_ub_offset;

    /* Cached func ptrs */
    PFNGLBINDBUFFERRANGEPROC     pGLBindBufferRange;
    PFNGLBLENDEQUATIONPROC       pGLBlendEquation;
    PFNGLBLENDFUNCPROC           pGLBlendFunc;
    PFNGLDISABLEPROC             pGLDisable;
    PFNGLDRAWARRAYSPROC          pGLDrawArrays;
    PFNGLENABLEPROC              pGLEnable;
    PFNGLUNIFORMBLOCKBINDINGPROC pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC          pGLUseProgram;
} _ogl_ui_frame;

/** Internal variables */
static const char* ui_frame_fragment_shader_body = "#version 420 core\n"
                                                   "\n"
                                                   "out vec4 result;\n"
                                                   "\n"
                                                   "void main()\n"
                                                   "{\n"
                                                   "    result = vec4(0.1, 0.1, 0.1f, 0.7);\n"
                                                   "}\n";

static system_hashed_ansi_string ui_frame_program_name = system_hashed_ansi_string_create("UI Frame");

/** TODO */
PRIVATE void _ogl_ui_frame_init_program(__in __notnull ogl_ui         ui,
                                        __in __notnull _ogl_ui_frame* frame_ptr)
{
    /* Create all objects */
    ogl_context context         = ogl_ui_get_context(ui);
    ogl_shader  fragment_shader = ogl_shader_create(context,
                                                    SHADER_TYPE_FRAGMENT,
                                                    system_hashed_ansi_string_create("UI frame fragment shader") );
    ogl_shader  vertex_shader   = ogl_shader_create(context,
                                                    SHADER_TYPE_VERTEX,
                                                    system_hashed_ansi_string_create("UI frame vertex shader") );

    frame_ptr->program = ogl_program_create(context,
                                            system_hashed_ansi_string_create("UI frame program"),
                                            OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    /* Set up shaders */
    ogl_shader_set_body(fragment_shader,
                        system_hashed_ansi_string_create(ui_frame_fragment_shader_body) );
    ogl_shader_set_body(vertex_shader,
                        system_hashed_ansi_string_create(ui_general_vertex_shader_body) );

    ogl_shader_compile(fragment_shader);
    ogl_shader_compile(vertex_shader);

    /* Set up program object */
    ogl_program_attach_shader(frame_ptr->program,
                              fragment_shader);
    ogl_program_attach_shader(frame_ptr->program,
                              vertex_shader);

    ogl_program_link(frame_ptr->program);

    /* Register the prgoram with UI so following frame instances will reuse the program */
    ogl_ui_register_program(ui,
                            ui_frame_program_name,
                            frame_ptr->program);

    /* Release shaders we will no longer need */
    ogl_shader_release(fragment_shader);
    ogl_shader_release(vertex_shader);
}

/** Please see header for specification */
PUBLIC void ogl_ui_frame_deinit(void* internal_instance)
{
    _ogl_ui_frame* ui_frame_ptr = (_ogl_ui_frame*) internal_instance;

    ogl_context_release(ui_frame_ptr->context);
    ogl_program_release(ui_frame_ptr->program);

    delete internal_instance;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_frame_draw(void* internal_instance)
{
    _ogl_ui_frame* frame_ptr = (_ogl_ui_frame*) internal_instance;

    /* Update uniforms */
    GLuint program_id = ogl_program_get_id(frame_ptr->program);

    ogl_program_ub_set_nonarrayed_uniform_value(frame_ptr->program_ub,
                                                frame_ptr->program_x1y1x2y2_ub_offset,
                                                frame_ptr->x1y1x2y2,
                                                0, /* src_data-flags */
                                                sizeof(float) * 4);

    /* Draw */
    frame_ptr->pGLBlendEquation(GL_FUNC_ADD);
    frame_ptr->pGLBlendFunc    (GL_SRC_ALPHA,
                                GL_ONE_MINUS_SRC_ALPHA);
    frame_ptr->pGLEnable       (GL_BLEND);
    {
        frame_ptr->pGLUseProgram     (ogl_program_get_id(frame_ptr->program) );
        frame_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                      0, /* index */
                                      frame_ptr->program_ub_bo_id,
                                      frame_ptr->program_ub_bo_start_offset,
                                      frame_ptr->program_ub_bo_size);

        ogl_program_ub_sync(frame_ptr->program_ub);

        frame_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                 0,  /* first */
                                 4); /* count */
    }
    frame_ptr->pGLDisable(GL_BLEND);
}

/** Please see header for specification */
PUBLIC void ogl_ui_frame_get_property(__in  __notnull const void*              frame,
                                      __in            _ogl_ui_control_property property,
                                      __out __notnull void*                    out_result)
{
    const _ogl_ui_frame* frame_ptr = (const _ogl_ui_frame*) frame;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED:
        {
            *(float*) out_result = frame_ptr->x1y1x2y2[3] - frame_ptr->x1y1x2y2[1];

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = frame_ptr->visible;

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2:
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
                              "Unrecognized _ogl_ui_control_property value");
        }
    } /* switch (property_value) */
}

/** Please see header for specification */
PUBLIC void* ogl_ui_frame_init(__in           __notnull ogl_ui       instance,
                               __in_ecount(4) __notnull const float* x1y1x2y2)
{
    _ogl_ui_frame* new_frame = new (std::nothrow) _ogl_ui_frame;

    ASSERT_ALWAYS_SYNC(new_frame != NULL,
                       "Out of memory");

    if (new_frame != NULL)
    {
        /* Initialize fields */
        memset(new_frame,
               0,
               sizeof(_ogl_ui_frame) );

        new_frame->context     = ogl_ui_get_context(instance);
        new_frame->visible     = true;
        new_frame->x1y1x2y2[0] = x1y1x2y2[0];
        new_frame->x1y1x2y2[1] = 1.0f - x1y1x2y2[1];
        new_frame->x1y1x2y2[2] = x1y1x2y2[2];
        new_frame->x1y1x2y2[3] = 1.0f - x1y1x2y2[3];

        ogl_context_retain(new_frame->context);

        /* Cache GL func pointers */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(new_frame->context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_frame->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_frame->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_frame->pGLBlendEquation       = entry_points->pGLBlendEquation;
            new_frame->pGLBlendFunc           = entry_points->pGLBlendFunc;
            new_frame->pGLDisable             = entry_points->pGLDisable;
            new_frame->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_frame->pGLEnable              = entry_points->pGLEnable;
            new_frame->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_frame->pGLUseProgram          = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_frame->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_frame->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_frame->pGLBlendEquation       = entry_points->pGLBlendEquation;
            new_frame->pGLBlendFunc           = entry_points->pGLBlendFunc;
            new_frame->pGLDisable             = entry_points->pGLDisable;
            new_frame->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_frame->pGLEnable              = entry_points->pGLEnable;
            new_frame->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_frame->pGLUseProgram          = entry_points->pGLUseProgram;
        }

        /* Retrieve the rendering program */
        new_frame->program = ogl_ui_get_registered_program(instance,
                                                           ui_frame_program_name);

        if (new_frame->program == NULL)
        {
            _ogl_ui_frame_init_program(instance,
                                       new_frame);

            ASSERT_DEBUG_SYNC(new_frame->program != NULL,
                              "Could not initialize frame UI program");
        } /* if (new_button->program == NULL) */

        /* Retrieve the uniform block properties */
        ogl_program_get_uniform_block_by_name(new_frame->program,
                                              system_hashed_ansi_string_create("dataVS"),
                                             &new_frame->program_ub);

        ASSERT_DEBUG_SYNC(new_frame->program_ub != NULL,
                          "dataVS uniform block descriptor is NULL");

        ogl_program_ub_get_property(new_frame->program_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &new_frame->program_ub_bo_size);
        ogl_program_ub_get_property(new_frame->program_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                   &new_frame->program_ub_bo_id);
        ogl_program_ub_get_property(new_frame->program_ub,
                                    OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                   &new_frame->program_ub_bo_start_offset);

        /* Set up UBO bindings */
        new_frame->pGLUniformBlockBinding(ogl_program_get_id(new_frame->program),
                                          0,  /* uniformBlockIndex */
                                          0); /* uniformBlockBinding */

        /* Retrieve the uniforms */
        const ogl_program_variable* x1y1x2y2_uniform = NULL;

        ogl_program_get_uniform_by_name(new_frame->program,
                                        system_hashed_ansi_string_create("x1y1x2y2"),
                                       &x1y1x2y2_uniform);

        new_frame->program_x1y1x2y2_ub_offset = x1y1x2y2_uniform->block_offset;
    } /* if (new_frame != NULL) */

    return (void*) new_frame;
}

/** Please see header for specification */
PUBLIC void ogl_ui_frame_set_property(__in __notnull void*                    frame,
                                      __in __notnull _ogl_ui_control_property property,
                                      __in __notnull const void*              data)
{
    _ogl_ui_frame* frame_ptr = (_ogl_ui_frame*) frame;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2:
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
                              "Unrecognized _ogl_ui_control_property value");
        }
    } /* switch (property_value) */
}