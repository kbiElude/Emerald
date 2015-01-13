/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_lights_preview.h"
#include "ogl/ogl_shader.h"
#include "scene/scene.h"
#include "system/system_matrix4x4.h"
#include <string>
#include <sstream>

/* This is pretty basic stuff to draw white splats in clip space */
#define POSITION_UNIFORM_LOCATION  (0)
#define COLOR_UNIFORM_LOCATION     (2)

static const char* preview_fragment_shader = "#version 420\n"
                                             "\n"
                                             "#extension GL_ARB_explicit_uniform_location : enable\n"
                                             "\n"
                                             "layout(location = 2) uniform vec3 color;\n"
                                             "\n"
                                             "out vec4 result;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    result = vec4(color, 1.0);\n"
                                             "}\n";
static const char* preview_vertex_shader   = "#version 420\n"
                                             "\n"
                                             "#extension GL_ARB_explicit_uniform_location : enable\n"
                                             "\n"
                                             "layout(location = 0) uniform vec4 position[2];\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    gl_Position = position[gl_VertexID];\n"
                                             "}\n";

/** TODO */
typedef struct _ogl_scene_renderer_lights_preview
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    ogl_program preview_program;
    scene       scene;
    GLuint      vao_id;

    _ogl_scene_renderer_lights_preview()
    {
        context         = NULL;
        preview_program = NULL;
        scene           = NULL;
        vao_id          = 0;
    }

} _ogl_scene_renderer_lights_preview;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_scene_renderer_lights_preview_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_scene_renderer_lights_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _ogl_context_scene_renderer_lights_preview_init_preview_program(__in __notnull _ogl_scene_renderer_lights_preview* preview_ptr)
{
    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == NULL,
                      "Preview program has already been initialized");

    /* Create shaders and set their bodies */
    system_hashed_ansi_string scene_name = NULL;

    scene_get_property(preview_ptr->scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    ogl_shader fs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_FRAGMENT,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview FS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );
    ogl_shader vs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_VERTEX,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview VS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );

    ogl_shader_set_body(fs_shader,
                        system_hashed_ansi_string_create(preview_fragment_shader) );
    ogl_shader_set_body(vs_shader,
                        system_hashed_ansi_string_create(preview_vertex_shader) );

    if (!ogl_shader_compile(fs_shader) ||
        !ogl_shader_compile(vs_shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to compile at least one of the shader used by lights preview renderer");

        goto end_fail;
    }

    /* Initialize the program object */
    preview_ptr->preview_program = ogl_program_create(preview_ptr->context,
                                                      system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview program ",
                                                                                                              system_hashed_ansi_string_get_buffer(scene_name)) );

    ogl_program_attach_shader(preview_ptr->preview_program,
                              fs_shader);
    ogl_program_attach_shader(preview_ptr->preview_program,
                              vs_shader);

    if (!ogl_program_link(preview_ptr->preview_program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to link the program object used by lights preview renderer");

        goto end_fail;
    }

    /* All done */
    goto end;

end_fail:
    if (preview_ptr->preview_program != NULL)
    {
        ogl_program_release(preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }

end:
    if (fs_shader != NULL)
    {
        ogl_shader_release(fs_shader);
    }

    if (vs_shader != NULL)
    {
        ogl_shader_release(vs_shader);
    }
}

/** TODO */
PRIVATE void _ogl_scene_renderer_lights_preview_release_renderer_callback(__in __notnull ogl_context context,
                                                                          __in __notnull void*       preview)
{
    const ogl_context_gl_entrypoints*   entrypoints_ptr = NULL;
    _ogl_scene_renderer_lights_preview* preview_ptr     = (_ogl_scene_renderer_lights_preview*) preview;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (preview_ptr->vao_id != 0)
    {
        entrypoints_ptr->pGLDeleteVertexArrays(1,
                                               &preview_ptr->vao_id);

        preview_ptr->vao_id = 0;
    }
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_scene_renderer_lights_preview_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_scene_renderer_lights_preview is only supported under GL contexts")
    }
#endif


/** Please see header for spec */
PUBLIC ogl_scene_renderer_lights_preview ogl_scene_renderer_lights_preview_create(__in __notnull ogl_context context,
                                                                                  __in __notnull scene       scene)
{
    _ogl_scene_renderer_lights_preview* new_instance = new (std::nothrow) _ogl_scene_renderer_lights_preview;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        /* Do not allocate any GL objects at this point. We will only create GL objects if we
         * are asked to render the preview.
         */
        new_instance->context         = context;
        new_instance->preview_program = NULL;
        new_instance->scene           = scene;
        new_instance->vao_id          = 0;

        scene_retain(scene);
    } /* if (new_instance != NULL) */

    return (ogl_scene_renderer_lights_preview) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_scene_renderer_lights_preview_release(__in __notnull __post_invalid ogl_scene_renderer_lights_preview preview)
{
    _ogl_scene_renderer_lights_preview* preview_ptr = (_ogl_scene_renderer_lights_preview*) preview;

    ogl_context_request_callback_from_context_thread(preview_ptr->context,
                                                     _ogl_scene_renderer_lights_preview_release_renderer_callback,
                                                     preview_ptr);

    if (preview_ptr->scene != NULL)
    {
        scene_release(preview_ptr->scene);
    }

    delete preview_ptr;
    preview_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_render(__in __notnull     ogl_scene_renderer_lights_preview preview,
                                                                            __in_ecount(4)     float*                            light_position,
                                                                            __in_ecount(3)     float*                            light_color,
                                                                            __in_ecount_opt(3) float*                            light_pos_plus_direction)
{
    const ogl_context_gl_entrypoints*   entrypoints_ptr = NULL;
    _ogl_scene_renderer_lights_preview* preview_ptr     = (_ogl_scene_renderer_lights_preview*) preview;
    const GLint                         program_id      = ogl_program_get_id(preview_ptr->preview_program);

    /* Retrieve mesh properties */
    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Set the uniforms */
    float merged_light_positions[8];

    memcpy(merged_light_positions,
           light_position,
           sizeof(float) * 4);

    if (light_pos_plus_direction != NULL)
    {
        memcpy(merged_light_positions + 4,
               light_pos_plus_direction,
               sizeof(float) * 4);
    }

    entrypoints_ptr->pGLProgramUniform4fv(program_id,
                                          POSITION_UNIFORM_LOCATION,
                                          (light_pos_plus_direction != NULL) ? 2 : 1,
                                          merged_light_positions);
    entrypoints_ptr->pGLProgramUniform3fv(program_id,
                                          COLOR_UNIFORM_LOCATION,
                                          1, /* count */
                                          light_color);

    /* Draw. This probably beats the world's worst way of achieving this functionality,
     * but light preview is only used for debugging purposes, so not much sense
     * in writing an overbloated implementation.
     */
    entrypoints_ptr->pGLDrawArrays(GL_POINTS,
                                   0,  /* first */
                                   1); /* count */

    if (light_pos_plus_direction != NULL)
    {
        entrypoints_ptr->pGLDrawArrays(GL_LINES,
                                       0,  /* first */
                                       2); /* count */
    }
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_start(__in __notnull ogl_scene_renderer_lights_preview preview)
{
    const ogl_context_gl_entrypoints*   entrypoints_ptr = NULL;
    _ogl_scene_renderer_lights_preview* preview_ptr     = (_ogl_scene_renderer_lights_preview*) preview;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Initialize the program object if one has not been initialized yet */
    if (preview_ptr->preview_program == NULL)
    {
        _ogl_context_scene_renderer_lights_preview_init_preview_program(preview_ptr);

        ASSERT_DEBUG_SYNC(preview_ptr->preview_program != NULL,
                          "Could not initialize preview program");
    }

    /* Initialize other GL objects */
    if (preview_ptr->vao_id == 0)
    {
        entrypoints_ptr->pGLGenVertexArrays(1,
                                            &preview_ptr->vao_id);
    }

    entrypoints_ptr->pGLBindVertexArray(preview_ptr->vao_id);
    entrypoints_ptr->pGLLineWidth      (4.0f);
    entrypoints_ptr->pGLPointSize      (16.0f);
    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(preview_ptr->preview_program) );
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_stop(__in __notnull ogl_scene_renderer_lights_preview preview)
{
    const ogl_context_gl_entrypoints*   entrypoints_ptr = NULL;
    _ogl_scene_renderer_lights_preview* preview_ptr     = (_ogl_scene_renderer_lights_preview*) preview;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLBindVertexArray(0);
}


