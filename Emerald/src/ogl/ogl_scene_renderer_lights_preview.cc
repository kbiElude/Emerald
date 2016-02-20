/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_lights_preview.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
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
    ral_program    preview_program;
    ogl_program_ub preview_program_ub;
    ral_buffer     preview_program_ub_bo;
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
    const ral_program_variable* color_uniform_ral_ptr     = NULL;
    const ral_program_variable* position_uniform_ral_ptr  = NULL;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == NULL,
                      "Preview program has already been initialized");

    /* Create shaders and set their bodies */
    ral_shader                      fs         = NULL;
    const system_hashed_ansi_string fs_body    = system_hashed_ansi_string_create(preview_fragment_shader);
    system_hashed_ansi_string       scene_name = NULL;
    ral_shader                      vs         = NULL;
    const system_hashed_ansi_string vs_body    = system_hashed_ansi_string_create(preview_vertex_shader);

    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview FS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview VS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_VERTEX
    };

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview program ",
                                                                system_hashed_ansi_string_get_buffer(scene_name))
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);

    ral_shader result_shaders[n_shader_create_info_items];


    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    if (!ral_context_create_shaders(preview_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed");

        goto end_fail;
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body);

    /* Initialize the program object */
    if (!ral_context_create_programs(preview_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &preview_ptr->preview_program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");

        goto end_fail;
    }

    if (!ral_program_attach_shader(preview_ptr->preview_program,
                                   fs) ||
        !ral_program_attach_shader(preview_ptr->preview_program,
                                   vs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");

        goto end_fail;
    }

    /* Retrieve UB properties */
    const raGL_program preview_program_raGL = ral_context_get_program_gl(preview_ptr->context,
                                                                         preview_ptr->preview_program);

    raGL_program_get_uniform_block_by_name(preview_program_raGL,
                                           system_hashed_ansi_string_create("data"),
                                          &preview_ptr->preview_program_ub);

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_ub != NULL,
                      "Data UB descriptor is NULL");

    ogl_program_ub_get_property(preview_ptr->preview_program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &preview_ptr->preview_program_ub_bo_size);
    ogl_program_ub_get_property(preview_ptr->preview_program_ub,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &preview_ptr->preview_program_ub_bo);

    /* Retrieve uniform properties */
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("color"),
                                          &color_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("position[0]"),
                                          &position_uniform_ral_ptr);

    preview_ptr->preview_program_color_ub_offset    = color_uniform_ral_ptr->block_offset;
    preview_ptr->preview_program_position_ub_offset = position_uniform_ral_ptr->block_offset;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_color_ub_offset    != -1 &&
                      preview_ptr->preview_program_position_ub_offset != -1,
                      "At least one uniform UB offset is -1");

    /* All done */
    goto end;

end_fail:
    if (preview_ptr->preview_program != NULL)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }

end:
    const ral_shader shaders_to_release[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               (const void**) shaders_to_release);
}


/** Please see header for spec */
PUBLIC ogl_scene_renderer_lights_preview ogl_scene_renderer_lights_preview_create(ral_context context,
                                                                                  scene       scene)
{
    _ogl_scene_renderer_lights_preview* new_instance_ptr = new (std::nothrow) _ogl_scene_renderer_lights_preview;

    ASSERT_ALWAYS_SYNC(new_instance_ptr != NULL,
                       "Out of memory");

    if (new_instance_ptr != NULL)
    {
        /* Do not allocate any GL objects at this point. We will only create GL objects if we
         * are asked to render the preview.
         */
        new_instance_ptr->context         = context;
        new_instance_ptr->owned_scene     = scene;
        new_instance_ptr->preview_program = NULL;

        scene_retain(scene);
    } /* if (new_instance != NULL) */

    return (ogl_scene_renderer_lights_preview) new_instance_ptr;
}

/** Please see header for spec */
PUBLIC void ogl_scene_renderer_lights_preview_release(ogl_scene_renderer_lights_preview preview)
{
    _ogl_scene_renderer_lights_preview* preview_ptr = (_ogl_scene_renderer_lights_preview*) preview;

    if (preview_ptr->preview_program != NULL)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &preview_ptr->preview_program);

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
    GLuint      preview_program_ub_bo_id           = 0;
    raGL_buffer preview_program_ub_bo_raGL         = NULL;
    uint32_t    preview_program_ub_bo_start_offset = -1;

    preview_program_ub_bo_raGL = ral_context_get_buffer_gl(preview_ptr->context,
                                                           preview_ptr->preview_program_ub_bo);

    raGL_buffer_get_property(preview_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &preview_program_ub_bo_id);
    raGL_buffer_get_property(preview_program_ub_bo_raGL,
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

    /* Initialize the program object if one has not been initialized yet */
    if (preview_ptr->preview_program == NULL)
    {
        _ogl_context_scene_renderer_lights_preview_init_preview_program(preview_ptr);

        ASSERT_DEBUG_SYNC(preview_ptr->preview_program != NULL,
                          "Could not initialize preview program");
    }


    const raGL_program program_raGL    = ral_context_get_program_gl(preview_ptr->context,
                                                                    preview_ptr->preview_program);
    GLuint             program_raGL_id = 0;


    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);
    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Initialize other GL objects */
    GLuint vao_id = 0;

    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    entrypoints_ptr->pGLBindVertexArray(vao_id);
    entrypoints_ptr->pGLLineWidth      (4.0f);
    entrypoints_ptr->pGLPointSize      (16.0f);
    entrypoints_ptr->pGLUseProgram     (program_raGL_id);
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


