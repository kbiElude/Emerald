/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "glsl/glsl_shader_constructor.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "scene_renderer/scene_renderer_sm.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>

/** Internal type definition */
typedef struct _shaders_vertex_uber_item
{
    shaders_vertex_uber_light     light; /* only used for light-specific items */
    shaders_vertex_uber_item_type type;

    _shaders_vertex_uber_item()
    {
        light = SHADERS_VERTEX_UBER_LIGHT_NONE;
        type  = SHADERS_VERTEX_UBER_ITEM_UNKNOWN;
    }
} _shaders_vertex_uber_item;

typedef struct
{
    ral_context             context;
    bool                    dirty;
    glsl_shader_constructor shader_constructor;
    ral_shader              vertex_shader;

    /** Holds _shaders_vertex_uber_item* instances. */
    system_resizable_vector added_items;

    glsl_shader_constructor_uniform_block_id vs_ub_id;

    REFCOUNT_INSERT_VARIABLES
} _shaders_vertex_uber;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_uber,
                               shaders_vertex_uber,
                              _shaders_vertex_uber);


/** TODO */
PRIVATE void _shaders_vertex_uber_add_sh3_light_support(glsl_shader_constructor                  shader_constructor,
                                                        unsigned int                             n_light,
                                                        glsl_shader_constructor_uniform_block_id vs_ub_id)
{
    /* Add UB variables */
    std::stringstream light_sh3_variable_name_sstream;

    light_sh3_variable_name_sstream << "light"
                                    << n_light
                                    << "_sh3";

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                       9, /* array_size */
                                                       vs_ub_id,
                                                       system_hashed_ansi_string_create(light_sh3_variable_name_sstream.str().c_str() ),
                                                       nullptr /* out_variable_id */);

    /* Add uniform variables */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_BUFFER,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("mesh_sh3"),
                                                       nullptr /* out_variable_id */);
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_INT,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("mesh_sh3_data_offset"),
                                                       nullptr /* out_variable_id */);

    /* Add output variables */
    std::stringstream light_mesh_ambient_diffuse_sh3_name_sstream;

    light_mesh_ambient_diffuse_sh3_name_sstream << "light"
                                                << n_light
                                                << "_mesh_ambient_diffuse_sh3";

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create(light_mesh_ambient_diffuse_sh3_name_sstream.str().c_str() ),
                                                       nullptr /* out_variable_id */);

    /* Compute output variable value */
    std::stringstream body;

    body << "light" << n_light << "_mesh_ambient_diffuse_sh3 = vec3(dot(light" << n_light << "_sh3[0].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9  ).xyz) +\n"
                                                                   "dot(light" << n_light << "_sh3[1].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+1).xyz) +\n"
                                                                   "dot(light" << n_light << "_sh3[2].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+2).xyz), \n"
                                                                   "dot(light" << n_light << "_sh3[3].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+3).xyz) +\n"
                                                                   "dot(light" << n_light << "_sh3[4].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+4).xyz) +\n"
                                                                   "dot(light" << n_light << "_sh3[5].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+5).xyz), \n"
                                                                   "dot(light" << n_light << "_sh3[6].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+6).xyz) +\n"
                                                                   "dot(light" << n_light << "_sh3[7].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+7).xyz) +\n"
                                                                   "dot(light" << n_light << "_sh3[8].xyz, texelFetch(mesh_sh3, mesh_sh3_data_offset + gl_VertexID * 9+8).xyz));\n";

    glsl_shader_constructor_append_to_function_body(shader_constructor,
                                                    0, /* main() */
                                                    system_hashed_ansi_string_create(body.str().c_str() ));
}

/** TODO */
PRIVATE void _shaders_vertex_uber_add_sh4_light_support(glsl_shader_constructor                  shader_constructor,
                                                        unsigned int                             n_light,
                                                        glsl_shader_constructor_uniform_block_id vs_ub_id)
{
    /* Add UB variables */
    std::stringstream light_sh3_variable_name_sstream;

    light_sh3_variable_name_sstream << "light"
                                    << n_light
                                    << "_sh3";

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                       12, /* array_size */
                                                       vs_ub_id,
                                                       system_hashed_ansi_string_create(light_sh3_variable_name_sstream.str().c_str() ),
                                                       nullptr /* out_variable_id */);

    /* Add uniform variables */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_BUFFER,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("mesh_sh4"),
                                                       nullptr /* out_variable_id */);
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_INT,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("mesh_sh4_data_offset"),
                                                       nullptr /* out_variable_id */);

    /* Add output variables */
    std::stringstream light_mesh_ambient_diffuse_sh4_name_sstream;

    light_mesh_ambient_diffuse_sh4_name_sstream << "light"
                                                << n_light
                                                << "_mesh_ambient_diffuse_sh4";

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create(light_mesh_ambient_diffuse_sh4_name_sstream.str().c_str() ),
                                                      nullptr /* out_variable_id */);

    /* Compute output variable value */
    std::stringstream body;

    body << "light" << n_light << "_mesh_ambient_diffuse_sh4 = vec3(dot(light" << n_light << "_sh4[0],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12   ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[1],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+1 ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[2],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+2 ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[3],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+3 )),\n"
                                                                   "dot(light" << n_light << "_sh4[4],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+4 ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[5],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+5 ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[6],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+6 ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[7],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+7 )),\n"
                                                                   "dot(light" << n_light << "_sh4[8],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+8 ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[9],  texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+9 ))  +\n"
                                                                   "dot(light" << n_light << "_sh4[10], texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+10)) +\n"
                                                                   "dot(light" << n_light << "_sh4[11], texelFetch(mesh_sh4, mesh_sh4_data_offset + gl_VertexID * 12+11)));\n";

    glsl_shader_constructor_append_to_function_body(shader_constructor,
                                                    0, /* main() */
                                                    system_hashed_ansi_string_create(body.str().c_str() ));
}


/** Function called back when reference counter drops to zero.
 *
 *  @param ptr Pointer to _shaders_vertex_uber instance.
 **/
PRIVATE void _shaders_vertex_uber_release(void* ptr)
{
    _shaders_vertex_uber* data_ptr = reinterpret_cast<_shaders_vertex_uber*>(ptr);

    if (data_ptr->added_items != nullptr)
    {
        _shaders_vertex_uber_item* item_ptr = nullptr;

        while (system_resizable_vector_pop(data_ptr->added_items, &item_ptr) )
        {
            delete item_ptr;
        }

        system_resizable_vector_release(data_ptr->added_items);

        data_ptr->added_items = nullptr;
    }

    if (data_ptr->shader_constructor != nullptr)
    {
        glsl_shader_constructor_release(data_ptr->shader_constructor);

        data_ptr->shader_constructor = nullptr;
    }

    if (data_ptr->vertex_shader != nullptr)
    {
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&data_ptr->vertex_shader) );

        data_ptr->vertex_shader = nullptr;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API void shaders_vertex_uber_add_passthrough_input_attribute(shaders_vertex_uber       uber,
                                                                            system_hashed_ansi_string in_attribute_name,
                                                                            ral_program_variable_type in_attribute_variable_type,
                                                                            system_hashed_ansi_string out_attribute_name)
{
    _shaders_vertex_uber* uber_ptr = reinterpret_cast<_shaders_vertex_uber*>(uber);

    /* Add input attribute */
    if (!glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                            VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                            LAYOUT_QUALIFIER_NONE,
                                                            in_attribute_variable_type,
                                                            0, /* array_size */
                                                            0, /* uniform_block */
                                                            in_attribute_name,
                                                            nullptr /* out_variable_id */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not add input attribute [%s] to the uber vertex shader",
                          system_hashed_ansi_string_get_buffer(in_attribute_name) );
    }

    /* Add output attribute */
    if (!glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                            VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                            LAYOUT_QUALIFIER_NONE,
                                                            in_attribute_variable_type,
                                                            0, /* array_size */
                                                            0, /* uniform_block */
                                                            out_attribute_name,
                                                            nullptr /* out_variable_id */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not add output attribute [%s] to the uber vertex shader",
                          system_hashed_ansi_string_get_buffer(out_attribute_name) );
    }

    /* Copy the input attribute's value to the output varying */
    system_hashed_ansi_string body_has;
    std::stringstream         body_sstream;

    body_sstream << system_hashed_ansi_string_get_buffer(out_attribute_name)
                 << " = "
                 << system_hashed_ansi_string_get_buffer(in_attribute_name)
                 << ";\n";

    body_has = system_hashed_ansi_string_create(body_sstream.str().c_str() );

    glsl_shader_constructor_append_to_function_body(uber_ptr->shader_constructor,
                                                    0, /* main() */
                                                    body_has);

    /* Reset the shader's body so any dependent program does not try to link with an outdated
     * version of the shader body. */
    ral_shader_set_property(uber_ptr->vertex_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                            nullptr);

    /* Mark the object as dirty */
    uber_ptr->dirty = true;
}

/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_uber_item_id shaders_vertex_uber_add_light(shaders_vertex_uber              uber,
                                                                             shaders_vertex_uber_light        light,
                                                                             shaders_fragment_uber_light_type light_type,
                                                                             bool                             is_shadow_caster)
{
    _shaders_vertex_uber*       uber_ptr     = reinterpret_cast<_shaders_vertex_uber*>(uber);
    unsigned int                n_items      = 0;
    _shaders_vertex_uber_item*  new_item_ptr = nullptr;
    shaders_vertex_uber_item_id result       = -1;

    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_items);

    switch (light)
    {
        case SHADERS_VERTEX_UBER_LIGHT_NONE:
        {
            break;
        }

        case SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS:
        {
            _shaders_vertex_uber_add_sh3_light_support(uber_ptr->shader_constructor,
                                                       n_items,
                                                       uber_ptr->vs_ub_id);

            break;
        }

        case SHADERS_VERTEX_UBER_LIGHT_SH_4_BANDS:
        {
            _shaders_vertex_uber_add_sh4_light_support(uber_ptr->shader_constructor,
                                                       n_items,
                                                       uber_ptr->vs_ub_id);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized light type [%d] requested", light);

            goto end;
        }
    }

    /* Add shadow mapping code if needed */
    if (is_shadow_caster)
    {
        scene_renderer_sm_adjust_vertex_uber_code(uber_ptr->shader_constructor,
                                                  n_items, /* n_light */
                                                  light_type,
                                                  uber_ptr->vs_ub_id,
                                                  system_hashed_ansi_string_create("world_vertex_temp") );
    }

    /* Spawn new descriptor */
    new_item_ptr = new (std::nothrow) _shaders_vertex_uber_item;

    ASSERT_ALWAYS_SYNC(new_item_ptr != nullptr,
                       "Out of memory");

    if (new_item_ptr == nullptr)
    {
        goto end;
    }

    new_item_ptr->light = light;
    new_item_ptr->type  = SHADERS_VERTEX_UBER_ITEM_LIGHT;

    /* Reset the shader's body so any dependent program does not try to link with an outdated
     * version of the shader body. */
    ral_shader_set_property(uber_ptr->vertex_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                            nullptr);

    /* Store the descriptor */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    uber_ptr->dirty = true;
    result          = n_items;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_uber shaders_vertex_uber_create(ral_context               context,
                                                                  system_hashed_ansi_string name)
{
    std::stringstream                        body_stream;
    system_hashed_ansi_string                ral_shader_instance_name = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                                " vertex uber");
    _shaders_vertex_uber*                    result_object_ptr        = nullptr;
    shaders_vertex_uber                      result_shader            = nullptr;
    glsl_shader_constructor                  shader_constructor       = nullptr;
    ral_shader_create_info                   shader_create_info;
    glsl_shader_constructor_uniform_block_id ub_id                    = -1;
    ral_shader                               vertex_shader            = nullptr;

    /* Make sure no other RAL shader instance lives under the name we're about to use */
    if ((ral_context_get_shader_by_name(context,
                                        ral_shader_instance_name)) != nullptr)
    {
        /* It is an error if a shader with the same name already exists. */
        ASSERT_DEBUG_SYNC(false,
                          "A RAL shader named [%s] already exists!",
                          system_hashed_ansi_string_get_buffer(ral_shader_instance_name) );

        goto end;
    }

    /* Spawn the shader constructor */
    shader_constructor = glsl_shader_constructor_create(RAL_SHADER_TYPE_VERTEX,
                                                        system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                " vertex uber"));

    if (shader_constructor == nullptr)
    {
        LOG_ERROR("Could not create a shader constructor instance");

        goto end;
    }

    /* Create VertexShaderProperties uniform block */
    ub_id = glsl_shader_constructor_add_uniform_block(shader_constructor,
                                                      LAYOUT_QUALIFIER_STD140,
                                                      system_hashed_ansi_string_create("VertexShaderProperties") );

    /* Add world_camera member */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                       0, /* array_size */
                                                       ub_id,
                                                       system_hashed_ansi_string_create("world_camera"),
                                                       nullptr /* out_variable_id */);

    /* Add object_normal input attribute */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("object_normal"),
                                                       nullptr /* out_variable_id */);

    /* Add world_vertex output attribute */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("world_vertex"),
                                                       nullptr /* out_variable_id */);

    /* Add out_vs_normal output attribute */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("out_vs_normal"),
                                                       nullptr /* out_variable_id */);

    /* Add view_vector output attribute */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("view_vector"),
                                                       nullptr /* out_variable_id */);

    /* Add object_vertex input attribute */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("object_vertex"),
                                                       nullptr /* out_variable_id */);

    /* Add vp member */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_ROW_MAJOR,
                                                      RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
                                                      0, /* array_size */
                                                      ub_id,
                                                      system_hashed_ansi_string_create("vp"),
                                                      nullptr /* out_variable_id */);

    /* Add model uniform */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_ROW_MAJOR,
                                                      RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
                                                      0, /* array_size */
                                                      ub_id,
                                                      system_hashed_ansi_string_create("model"),
                                                      nullptr /* out_variable_id */);

    /* Add normal_matrix uniform */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_UNIFORM,
                                                       LAYOUT_QUALIFIER_ROW_MAJOR,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4,
                                                       0, /* array_size */
                                                       ub_id,
                                                       system_hashed_ansi_string_create("normal_matrix"),
                                                       nullptr /* out_variable_id */);

    /* Set the body */
    glsl_shader_constructor_set_function_body(shader_constructor,
                                              0, /* main() */
                                              system_hashed_ansi_string_create("vec4 world_vertex_temp = model * object_vertex;\n"
                                                                               "\n"
                                                                               "gl_Position   = vp * world_vertex_temp;\n"
                                                                               "world_vertex  = world_vertex_temp.xyz;\n"
                                                                               "out_vs_normal = normalize((normal_matrix * vec4(object_normal, 0.0) ).xyz);\n"
                                                                               "view_vector   = world_camera.xyz - world_vertex.xyz;\n"
                                                                               "\n"));

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_vertex_uber;

    ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                      "Out of memory while instantiating _shaders_vertex_uber object.");

    if (result_object_ptr == nullptr)
    {
        LOG_ERROR("Out of memory while creating uber vertex shader object instance.");

        goto end;
    }

    /* Create the shader */
    system_hashed_ansi_string shader_body(glsl_shader_constructor_get_shader_body(shader_constructor) );

    shader_create_info.name   = ral_shader_instance_name;
    shader_create_info.source = RAL_SHADER_SOURCE_GLSL;
    shader_create_info.type   = RAL_SHADER_TYPE_VERTEX;

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &vertex_shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a new RAL shader instance");

        goto end;
    }

    ral_shader_set_property(vertex_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Fill other fields */
    memset(result_object_ptr,
           0,
           sizeof(_shaders_vertex_uber) );

    result_object_ptr->added_items        = system_resizable_vector_create(4 /* capacity */);
    result_object_ptr->context            = context;
    result_object_ptr->dirty              = true;
    result_object_ptr->shader_constructor = shader_constructor;
    result_object_ptr->vertex_shader      = vertex_shader;
    result_object_ptr->vs_ub_id           = ub_id;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_vertex_uber_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_UBER,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Uber Vertex Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_uber) result_object_ptr;

end:
    if (shader_constructor != nullptr)
    {
        glsl_shader_constructor_release(shader_constructor);

        shader_constructor = nullptr;
    }

    return nullptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_item_type(shaders_vertex_uber            uber,
                                                          shaders_vertex_uber_item_id    item_id,
                                                          shaders_vertex_uber_item_type* out_item_type)
{
    _shaders_vertex_uber_item* item_ptr = nullptr;
    _shaders_vertex_uber*      uber_ptr = reinterpret_cast<_shaders_vertex_uber*>(uber);
    bool                       result   = false;

    if (!system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                item_id,
                                               &item_ptr) )
    {
        LOG_ERROR("Could not retrieve uber vertex shader item type at index [%u]",
                  item_id);

        goto end;
    }

    *out_item_type = item_ptr->type;
    result         = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_light_type(shaders_vertex_uber         uber,
                                                           shaders_vertex_uber_item_id item_id,
                                                           shaders_vertex_uber_light*  out_light_type)
{
    _shaders_vertex_uber_item* item_ptr = nullptr;
    _shaders_vertex_uber*      uber_ptr = reinterpret_cast<_shaders_vertex_uber*>(uber);
    bool                       result   = false;

    if (!system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                item_id,
                                               &item_ptr) )
    {
        LOG_ERROR("Could not retrieve uber vertex shader item type at index [%u]",
                  item_id);

        goto end;
    }

    if (item_ptr->type != SHADERS_VERTEX_UBER_ITEM_LIGHT)
    {
        LOG_ERROR("Uber vertex shader item at index [%u] is not a light.",
                  item_id);

        goto end;
    }

    *out_light_type = item_ptr->light;
    result          = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t shaders_vertex_uber_get_n_items(shaders_vertex_uber shader)
{
    uint32_t result = 0;

    system_resizable_vector_get_property(((_shaders_vertex_uber*)shader)->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_vertex_uber_get_shader(shaders_vertex_uber shader)
{
    _shaders_vertex_uber* shader_ptr = reinterpret_cast<_shaders_vertex_uber*>(shader);

    if (shader_ptr->dirty)
    {
        LOG_INFO("Uber vertex shader marked as dirty - recompiling");

        shaders_vertex_uber_recompile(shader);
    }

    return shader_ptr->vertex_shader;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool shaders_vertex_uber_is_dirty(shaders_vertex_uber uber)
{
    return ((_shaders_vertex_uber*) uber)->dirty;
}

/* Please see header for specificationb */
PUBLIC EMERALD_API void shaders_vertex_uber_recompile(shaders_vertex_uber uber)
{
    _shaders_vertex_uber* uber_ptr = reinterpret_cast<_shaders_vertex_uber*>(uber);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(uber_ptr->dirty,
                      "shaders_vertex_uber_recompile() failed for non-dirty object");

    /* Set the shader's body */
    system_hashed_ansi_string shader_body = glsl_shader_constructor_get_shader_body(uber_ptr->shader_constructor);

    ral_shader_set_property(uber_ptr->vertex_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* All done */
    uber_ptr->dirty = false;
}