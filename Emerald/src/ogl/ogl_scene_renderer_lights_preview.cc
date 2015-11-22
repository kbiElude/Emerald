/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_lights_preview.h"
#include "ogl/ogl_shader.h"
#include "raGL/raGL_buffer.h"
#include "ral/ral_context.h"
#include "scene/scene.h"
#include "system/system_matrix4x4.h"
#include <string.h>

/* This is pretty basic stuff to draw white splats in clip space */
static const char* preview_fragment_shader = "#version 430 core\n"
                                             "\n"
                                             "uniform data\n"
                                             "{\n"
                                             "    vec3 color;\n"
                                             "    vec4 position[2];\n"
                                             "};\n"
                                             "\n"
                                             "out vec4 result;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    result = vec4(color, 1.0);\n"
                                             "}\n";
static const char* preview_vertex_shader   = "#version 430 core\n"
                                             "\n"
                                             "uniform data\n"
                                             "{\n"
                                             "    vec3 color;\n"
                                             "    vec4 position[2];\n"
                                             "};\n"
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
    ral_context context;

    scene          owned_scene;
    ogl_program    preview_program;
    ogl_program_ub preview_program_ub;
    raGL_buffer    preview_program_ub_bo;
    unsigned int   preview_program_ub_bo_size;
    GLint          preview_program_color_ub_offset;
    GLint          preview_program_position_ub_offset;

    _ogl_scene_renderer_lights_preview()
    {
        context                            = NULL;
        owned_scene                        = NULL;
        preview_program                    = NULL;
        preview_program_ub                 = NULL;
        preview_program_ub_bo              = NULL;
        preview_program_ub_bo_size         = 0;
        preview_program_color_ub_offset    = -1;
        preview_program_position_ub_offset = -1;
    }

} _ogl_scene_renderer_lights_preview;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_scene_renderer_lights_preview_verify_context_type(ogl_context);
#else
    #define _ogl_scene_renderer_lights_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _ogl_context_scene_renderer_lights_preview_init_preview_program(_ogl_scene_renderer_lights_preview* preview_ptr)
{
    const ogl_program_variable* color_uniform_ptr    = NULL;
    const ogl_program_variable* position_uniform_ptr = NULL;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == NULL,
                      "Preview program has already been initialized");

    /* Create shaders and set their bodies */
    ogl_shader                fs_shader  = NULL;
    system_hashed_ansi_string scene_name = NULL;
    ogl_shader                vs_shader  = NULL;

    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    fs_shader = ogl_shader_create(ral_context_get_gl_context(preview_ptr->context),
                                  RAL_SHADER_TYPE_FRAGMENT,
                                  system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview FS shader ",
                                                                                          system_hashed_ansi_string_get_buffer(scene_name)) );
    vs_shader = ogl_shader_create(ral_context_get_gl_context(preview_ptr->context),
                                  RAL_SHADER_TYPE_VERTEX,
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
    preview_ptr->preview_program = ogl_program_create(ral_context_get_gl_context(preview_ptr->context),
                                                      system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview program ",
                                                                                                              system_hashed_ansi_string_get_buffer(scene_name)),
                                                      OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

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

    /* Retrieve UB properties */
    ogl_program_get_uniform_block_by_name(preview_ptr->preview_program,
                                          system_hashed_ansi_string_create("data"),
                                         &preview_ptr->preview_program_ub);

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_ub != NULL,
                      "Data UB descriptor is NULL");

    ogl_program_ub_get_property(preview_ptr->preview_program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &preview_ptr->preview_program_ub_bo_size);
    ogl_program_ub_get_property(preview_ptr->preview_program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO,
                               &preview_ptr->preview_program_ub_bo);

    /* Retrieve uniform properties */
    ogl_program_get_uniform_by_name(preview_ptr->preview_program,
                                    system_hashed_ansi_string_create("color"),
                                   &color_uniform_ptr);
    ogl_program_get_uniform_by_name(preview_ptr->preview_program,
                                    system_hashed_ansi_string_create("position[0]"),
                                   &position_uniform_ptr);

    preview_ptr->preview_program_color_ub_offset    = color_uniform_ptr->block_offset;
    preview_ptr->preview_program_position_ub_offset = position_uniform_ptr->block_offset;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_color_ub_offset    != -1 &&
                      preview_ptr->preview_program_position_ub_offset != -1,
                      "At least one uniform UB offset is -1");

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
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_scene_renderer_lights_preview_verify_context_type(ogl_context context)
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
PUBLIC ogl_scene_renderer_lights_preview ogl_scene_renderer_lights_preview_create(ral_context context,
                                                                                  scene       scene)
{
    _ogl_scene_renderer_lights_preview* new_instance = new (std::nothrow) _ogl_scene_renderer_lights_preview;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    if (new_instance != NULL)
    {
        /* Do not allocate any GL objects at this point. We will only create GL objects if we
         * are asked to render the preview.
         */
        new_instance->context         = context;
        new_instance->owned_scene     = scene;
        new_instance->preview_program = NULL;

        scene_retain(scene);
    } /* if (new_instance != NULL) */

    return (ogl_scene_renderer_lights_preview) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_scene_renderer_lights_preview_release(ogl_scene_renderer_lights_preview preview)
{
    _ogl_scene_renderer_lights_preview* preview_ptr = (_ogl_scene_renderer_lights_preview*) preview;

    if (preview_ptr->preview_program != NULL)
    {
        ogl_program_release(preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }

    if (preview_ptr->owned_scene != NULL)
    {
        scene_release(preview_ptr->owned_scene);

        preview_ptr->owned_scene = NULL;
    }

    delete preview_ptr;
    preview_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_render(ogl_scene_renderer_lights_preview preview,
                                                                            float*                            light_position,
                                                                            float*                            light_color,
                                                                            float*                            light_pos_plus_direction)
{
    const ogl_context_gl_entrypoints*   entrypoints_ptr = NULL;
    _ogl_scene_renderer_lights_preview* preview_ptr     = (_ogl_scene_renderer_lights_preview*) preview;
    const GLint                         program_id      = ogl_program_get_id(preview_ptr->preview_program);

    /* Retrieve mesh properties */
    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
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

    ogl_program_ub_set_arrayed_uniform_value   (preview_ptr->preview_program_ub,
                                                preview_ptr->preview_program_position_ub_offset,
                                                merged_light_positions,
                                                0, /* src_data_flags */
                                                sizeof(float) * 4,
                                                0, /* dst_array_start_index */
                                                ((light_pos_plus_direction != NULL) ? 2 : 1) );
    ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->preview_program_ub,
                                                preview_ptr->preview_program_color_ub_offset,
                                                light_color,
                                                0, /* src_data_flags */
                                                sizeof(float) * 3);

    ogl_program_ub_sync(preview_ptr->preview_program_ub);

    /* Draw. This probably beats the world's worst way of achieving this functionality,
     * but light preview is only used for debugging purposes, so not much sense
     * in writing an overbloated implementation.
     */
    GLuint   preview_program_ub_bo_id           = 0;
    uint32_t preview_program_ub_bo_start_offset = -1;

    raGL_buffer_get_property(preview_ptr->preview_program_ub_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &preview_program_ub_bo_id);
    raGL_buffer_get_property(preview_ptr->preview_program_ub_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &preview_program_ub_bo_start_offset);

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        preview_program_ub_bo_id,
                                        preview_program_ub_bo_start_offset,
                                        preview_ptr->preview_program_ub_bo_size);

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
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_start(ogl_scene_renderer_lights_preview preview)
{
    const ogl_context_gl_entrypoints*   entrypoints_ptr = NULL;
    _ogl_scene_renderer_lights_preview* preview_ptr     = (_ogl_scene_renderer_lights_preview*) preview;

    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
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
    GLuint vao_id = 0;

    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    entrypoints_ptr->pGLBindVertexArray(vao_id);
    entrypoints_ptr->pGLLineWidth      (4.0f);
    entrypoints_ptr->pGLPointSize      (16.0f);
    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(preview_ptr->preview_program) );
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_stop(ogl_scene_renderer_lights_preview preview)
{
    const ogl_context_gl_entrypoints*   entrypoints_ptr = NULL;
    _ogl_scene_renderer_lights_preview* preview_ptr     = (_ogl_scene_renderer_lights_preview*) preview;

    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLBindVertexArray(0);
}


