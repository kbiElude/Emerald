/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shader_constructor.h"
#include "ogl/ogl_shadow_mapping.h"
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
    bool                     dirty;
    ogl_shader_constructor   shader_constructor;
    ogl_shader               vertex_shader;
    shaders_vertex_uber_type vs_type;

    /** Holds _shaders_vertex_uber_item* instances. */
    system_resizable_vector added_items;

    _uniform_block_id vs_ub_id;

    REFCOUNT_INSERT_VARIABLES
} _shaders_vertex_uber;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_uber,
                               shaders_vertex_uber,
                              _shaders_vertex_uber);


/** TODO */
PRIVATE void _shaders_vertex_uber_add_sh3_light_support(__in __notnull ogl_shader_constructor shader_constructor,
                                                        __in           unsigned int           n_light,
                                                        __in           _uniform_block_id      vs_ub_id)
{
    /* Add UB variables */
    std::stringstream light_sh3_variable_name_sstream;

    light_sh3_variable_name_sstream << "light"
                                    << n_light
                                    << "_sh3";

    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_VEC4,
                                                      9, /* array_size */
                                                      vs_ub_id,
                                                      system_hashed_ansi_string_create(light_sh3_variable_name_sstream.str().c_str() ),
                                                      NULL /* out_variable_id */);

    /* Add uniform variables */
    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_SAMPLERBUFFER,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create("mesh_sh3"),
                                                      NULL /* out_variable_id */);
    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_INT,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create("mesh_sh3_data_offset"),
                                                      NULL /* out_variable_id */);

    /* Add output variables */
    std::stringstream light_mesh_ambient_diffuse_sh3_name_sstream;

    light_mesh_ambient_diffuse_sh3_name_sstream << "light"
                                                << n_light
                                                << "_mesh_ambient_diffuse_sh3";

    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_VEC3,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create(light_mesh_ambient_diffuse_sh3_name_sstream.str().c_str() ),
                                                      NULL /* out_variable_id */);

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

    ogl_shader_constructor_append_to_function_body(shader_constructor,
                                                   0, /* main() */
                                                   system_hashed_ansi_string_create(body.str().c_str() ));
}

/** TODO */
PRIVATE void _shaders_vertex_uber_add_sh4_light_support(__in __notnull ogl_shader_constructor shader_constructor,
                                                        __in           unsigned int           n_light,
                                                        __in           _uniform_block_id      vs_ub_id)
{
    /* Add UB variables */
    std::stringstream light_sh3_variable_name_sstream;

    light_sh3_variable_name_sstream << "light"
                                    << n_light
                                    << "_sh3";

    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_VEC4,
                                                      12, /* array_size */
                                                      vs_ub_id,
                                                      system_hashed_ansi_string_create(light_sh3_variable_name_sstream.str().c_str() ),
                                                      NULL /* out_variable_id */);

    /* Add uniform variables */
    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_SAMPLERBUFFER,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create("mesh_sh4"),
                                                      NULL /* out_variable_id */);
    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_INT,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create("mesh_sh4_data_offset"),
                                                      NULL /* out_variable_id */);

    /* Add output variables */
    std::stringstream light_mesh_ambient_diffuse_sh4_name_sstream;

    light_mesh_ambient_diffuse_sh4_name_sstream << "light"
                                                << n_light
                                                << "_mesh_ambient_diffuse_sh4";

    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_VEC3,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create(light_mesh_ambient_diffuse_sh4_name_sstream.str().c_str() ),
                                                      NULL /* out_variable_id */);

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

    ogl_shader_constructor_append_to_function_body(shader_constructor,
                                                   0, /* main() */
                                                   system_hashed_ansi_string_create(body.str().c_str() ));
}


/** Function called back when reference counter drops to zero.
 *
 *  @param ptr Pointer to _shaders_vertex_uber instance.
 **/
PRIVATE void _shaders_vertex_uber_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_vertex_uber* data_ptr = (_shaders_vertex_uber*) ptr;

    if (data_ptr->added_items != NULL)
    {
        _shaders_vertex_uber_item* item_ptr = NULL;

        while (system_resizable_vector_pop(data_ptr->added_items, &item_ptr) )
        {
            delete item_ptr;
        }

        system_resizable_vector_release(data_ptr->added_items);

        data_ptr->added_items = NULL;
    }

    if (data_ptr->shader_constructor != NULL)
    {
        ogl_shader_constructor_release(data_ptr->shader_constructor);

        data_ptr->shader_constructor = NULL;
    }

    if (data_ptr->vertex_shader != NULL)
    {
        ogl_shader_release(data_ptr->vertex_shader);

        data_ptr->vertex_shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API void shaders_vertex_uber_add_passthrough_input_attribute(__in __notnull shaders_vertex_uber       uber,
                                                                            __in __notnull system_hashed_ansi_string in_attribute_name,
                                                                            __in __notnull _shader_variable_type     in_attribute_variable_type,
                                                                            __in __notnull system_hashed_ansi_string out_attribute_name)
{
    _shaders_vertex_uber* uber_ptr = (_shaders_vertex_uber*) uber;

    /* Add input attribute */
    if (!ogl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                           VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           in_attribute_variable_type,
                                                           0, /* array_size */
                                                           0, /* uniform_block */
                                                           in_attribute_name,
                                                           NULL /* out_variable_id */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not add input attribute [%s] to the uber vertex shader",
                          system_hashed_ansi_string_get_buffer(in_attribute_name) );
    }

    /* Add output attribute */
    if (!ogl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                           VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           in_attribute_variable_type,
                                                           0, /* array_size */
                                                           0, /* uniform_block */
                                                           out_attribute_name,
                                                           NULL /* out_variable_id */) )
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

    ogl_shader_constructor_append_to_function_body(uber_ptr->shader_constructor,
                                                   0, /* main() */
                                                   body_has);

    /* Mark the object as dirty */
    uber_ptr->dirty = true;
}

/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_uber_item_id shaders_vertex_uber_add_light(__in __notnull shaders_vertex_uber              uber,
                                                                             __in           shaders_vertex_uber_light        light,
                                                                             __in           shaders_fragment_uber_light_type light_type,
                                                                             __in           bool                             is_shadow_caster)
{
    _shaders_vertex_uber*       uber_ptr = (_shaders_vertex_uber*) uber;
    const unsigned int          n_items  = system_resizable_vector_get_amount_of_elements(uber_ptr->added_items);
    shaders_vertex_uber_item_id result   = -1;

    ASSERT_DEBUG_SYNC(uber_ptr->vs_type == SHADERS_VERTEX_UBER_TYPE_DEFAULT,
                      "Light items cannot be added to an uber VS of non-default type.");

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
    } /* switch (light) */

    /* Add shadow mapping code if needed */
    if (is_shadow_caster)
    {
        ogl_shadow_mapping_adjust_vertex_uber_code(uber_ptr->shader_constructor,
                                                   n_items, /* n_light */
                                                   light_type,
                                                   uber_ptr->vs_ub_id,
                                                   system_hashed_ansi_string_create("world_vertex_temp") );
    }

    /* Spawn new descriptor */
    _shaders_vertex_uber_item* new_item_ptr = new (std::nothrow) _shaders_vertex_uber_item;

    ASSERT_ALWAYS_SYNC(new_item_ptr != NULL, "Out of memory");
    if (new_item_ptr == NULL)
    {
        goto end;
    }

    new_item_ptr->light = light;
    new_item_ptr->type  = SHADERS_VERTEX_UBER_ITEM_LIGHT;

    /* Store the descriptor */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    uber_ptr->dirty = true;
    result          = n_items;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_uber shaders_vertex_uber_create(__in __notnull ogl_context               context,
                                                                  __in __notnull system_hashed_ansi_string name,
                                                                  __in           shaders_vertex_uber_type  vs_type)
{
    std::stringstream     body_stream;
    _shaders_vertex_uber* result_object = NULL;
    shaders_vertex_uber   result_shader = NULL;

    /* Spawn the shader constructor */
    ogl_shader_constructor shader_constructor = NULL;

    shader_constructor = ogl_shader_constructor_create(SHADER_TYPE_VERTEX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                               " vertex uber"));

    if (shader_constructor == NULL)
    {
        LOG_ERROR("Could not create a shader constructor instance");

        goto end;
    }

    /* Create VertexShaderProperties uniform block */
    _uniform_block_id ub_id = -1;

    ub_id = ogl_shader_constructor_add_uniform_block(shader_constructor,
                                                     LAYOUT_QUALIFIER_STD140,
                                                     system_hashed_ansi_string_create("VertexShaderProperties") );

    if (vs_type == SHADERS_VERTEX_UBER_TYPE_DEFAULT)
    {
        /* Add world_camera member */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_VEC4,
                                                          0, /* array_size */
                                                          ub_id,
                                                          system_hashed_ansi_string_create("world_camera"),
                                                          NULL /* out_variable_id */);

        /* Add object_normal input attribute */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_VEC3,
                                                          0, /* array_size */
                                                          0, /* uniform_block */
                                                          system_hashed_ansi_string_create("object_normal"),
                                                          NULL /* out_variable_id */);

        /* Add world_vertex output attribute */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_VEC3,
                                                          0, /* array_size */
                                                          0, /* uniform_block */
                                                          system_hashed_ansi_string_create("world_vertex"),
                                                          NULL /* out_variable_id */);

        /* Add out_vs_normal output attribute */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_VEC3,
                                                          0, /* array_size */
                                                          0, /* uniform_block */
                                                          system_hashed_ansi_string_create("out_vs_normal"),
                                                          NULL /* out_variable_id */);

        /* Add view_vector output attribute */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_VEC3,
                                                          0, /* array_size */
                                                          0, /* uniform_block */
                                                          system_hashed_ansi_string_create("view_vector"),
                                                          NULL /* out_variable_id */);
    }
    else
    if (vs_type == SHADERS_VERTEX_UBER_TYPE_DUAL_PARABOLOID_SM)
    {
        /* Add clip depth output variable, so that fragments outside the shadow map
         * can be discarded */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_FLOAT,
                                                          0, /* array_size */
                                                          ub_id,
                                                          system_hashed_ansi_string_create("fs_clip_depth"),
                                                          NULL /* out_variable_id */);

        /* Add flip_z uniform */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_FLOAT,
                                                          0, /* array_size */
                                                          ub_id,
                                                          system_hashed_ansi_string_create("flip_z"),
                                                          NULL /* out_variable_id */);

        /* Add near_plane uniform */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_FLOAT,
                                                          0, /* array_size */
                                                          ub_id,
                                                          system_hashed_ansi_string_create("near_plane"),
                                                          NULL /* out_variable_id */);

        /* Add far_near_plane_diff uniform */
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_NONE,
                                                          TYPE_FLOAT,
                                                          0, /* array_size */
                                                          ub_id,
                                                          system_hashed_ansi_string_create("far_near_plane_diff"),
                                                          NULL /* out_variable_id */);
    }

    /* Add object_vertex input attribute */
    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                      LAYOUT_QUALIFIER_NONE,
                                                      TYPE_VEC3,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create("object_vertex"),
                                                      NULL /* out_variable_id */);

    /* Add vp member */
    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_ROW_MAJOR,
                                                      TYPE_MAT4,
                                                      0, /* array_size */
                                                      ub_id,
                                                      system_hashed_ansi_string_create("vp"),
                                                      NULL /* out_variable_id */);

    /* Add global model uniform */
    ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                      VARIABLE_TYPE_UNIFORM,
                                                      LAYOUT_QUALIFIER_NONE, /* global UB, so we must use column-major ordering */
                                                      TYPE_MAT4,
                                                      0, /* array_size */
                                                      0, /* uniform_block */
                                                      system_hashed_ansi_string_create("model"),
                                                      NULL /* out_variable_id */);

    /* Add global normal_matrix uniform */
    if (vs_type == SHADERS_VERTEX_UBER_TYPE_DEFAULT)
    {
        ogl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                          VARIABLE_TYPE_UNIFORM,
                                                          LAYOUT_QUALIFIER_NONE, /* default UB - must use column-major ordering */
                                                          TYPE_MAT4,
                                                          0, /* array_size */
                                                          0, /* uniform_block */
                                                          system_hashed_ansi_string_create("normal_matrix"),
                                                          NULL /* out_variable_id */);
    }

    /* Set general body */
    switch (vs_type)
    {
        case SHADERS_VERTEX_UBER_TYPE_DUAL_PARABOLOID_SM:
        {
            ogl_shader_constructor_set_function_body(shader_constructor,
                                                     0, /* main() */
                                                     system_hashed_ansi_string_create("vec4 world_vertex = vp * model * vec4(object_vertex, 1.0);\n"
                                                                                      "vec4 clip_vertex  = world_vertex;\n"
                                                                                      "\n"
                                                                                      "clip_vertex.z *= flip_z;\n"
                                                                                      "\n"
                                                                                      "float light_to_vertex_vec_len = length(clip_vertex.xyz);\n"
                                                                                      "\n"
                                                                                      "clip_vertex.xyz /= vec3(light_to_vertex_vec_len);\n"
                                                                                      "clip_vertex.xy  /= vec2(clip_vertex.z + 1.0);\n"
                                                                                      "clip_vertex.z    = (light_to_vertex_vec_len - near_plane) / far_near_plane_diff * 2.0 - 1.0;\n"
                                                                                      "clip_vertex.w    = 1.0;\n"
                                                                                      "\n"
                                                                                      "gl_Position = clip_vertex;\n") );

            break;
        }

        case SHADERS_VERTEX_UBER_TYPE_DEFAULT:
        {
            ogl_shader_constructor_set_function_body(shader_constructor,
                                                     0, /* main() */
                                                     system_hashed_ansi_string_create("vec4 world_vertex_temp = model * vec4(object_vertex, 1.0);\n"
                                                                                      "\n"
                                                                                      "gl_Position   = vp * world_vertex_temp;\n"
                                                                                      "world_vertex  = world_vertex_temp.xyz;\n"
                                                                                      "out_vs_normal = normalize((normal_matrix * vec4(object_normal, 0.0) ).xyz);\n"
                                                                                      "view_vector   = normalize(world_camera.xyz - world_vertex.xyz);\n"
                                                                                      "\n"));

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized uber vertex shader type");
        }
    } /* switch (vs_type) */

    /* Create the shader */
    ogl_shader vertex_shader = ogl_shader_create(context,
                                                 SHADER_TYPE_VERTEX,
                                                 system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                         " vertex uber"));

    ASSERT_DEBUG_SYNC(vertex_shader != NULL,
                      "ogl_shader_create() failed");
    if (vertex_shader == NULL)
    {
        LOG_ERROR("Could not create uber vertex shader.");

        goto end;
    }

    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_vertex_uber;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_vertex_uber object.");
    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating uber vertex shader object instance.");

        goto end;
    }

    /* Fill other fields */
    memset(result_object,
           0,
           sizeof(_shaders_vertex_uber) );

    result_object->added_items        = system_resizable_vector_create(4 /* capacity */,
                                                                       sizeof(_shaders_vertex_uber_item*) );
    result_object->dirty              = true;
    result_object->shader_constructor = shader_constructor;
    result_object->vertex_shader      = vertex_shader;
    result_object->vs_ub_id           = ub_id;
    result_object->vs_type            = vs_type;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_vertex_uber_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_UBER,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Uber Vertex Shaders\\", system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_uber) result_object;

end:
    if (shader_constructor != NULL)
    {
        ogl_shader_constructor_release(shader_constructor);

        shader_constructor = NULL;
    }

    if (vertex_shader != NULL)
    {
        ogl_shader_release(vertex_shader);

        vertex_shader = NULL;
    }

    if (result_object != NULL)
    {
        delete result_object;

        result_object = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_item_type(__in __notnull shaders_vertex_uber            uber,
                                                          __in           shaders_vertex_uber_item_id    item_id,
                                                          __out          shaders_vertex_uber_item_type* out_item_type)
{
    _shaders_vertex_uber_item* item_ptr = NULL;
    _shaders_vertex_uber*      uber_ptr = (_shaders_vertex_uber*) uber;
    bool                       result   = false;

    if (!system_resizable_vector_get_element_at(uber_ptr->added_items, item_id, &item_ptr) )
    {
        LOG_ERROR("Could not retrieve uber vertex shader item type at index [%d]", item_id);

        goto end;
    }

    *out_item_type = item_ptr->type;
    result         = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_light_type(__in  __notnull shaders_vertex_uber         uber,
                                                           __in            shaders_vertex_uber_item_id item_id,
                                                           __out           shaders_vertex_uber_light*  out_light_type)
{
    _shaders_vertex_uber_item* item_ptr = NULL;
    _shaders_vertex_uber*      uber_ptr = (_shaders_vertex_uber*) uber;
    bool                       result   = false;

    if (!system_resizable_vector_get_element_at(uber_ptr->added_items,
                                                item_id,
                                               &item_ptr) )
    {
        LOG_ERROR("Could not retrieve uber vertex shader item type at index [%d]",
                  item_id);

        goto end;
    }

    if (item_ptr->type != SHADERS_VERTEX_UBER_ITEM_LIGHT)
    {
        LOG_ERROR("Uber vertex shader item at index [%d] is not a light.",
                  item_id);

        goto end;
    }

    *out_light_type = item_ptr->light;
    result          = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC shaders_vertex_uber_type shaders_vertex_uber_get_vs_uber_type_for_vs_behavior(__in mesh_material_vs_behavior material_vs_behavior)
{
    shaders_vertex_uber_type result;

    switch (material_vs_behavior)
    {
        case MESH_MATERIAL_VS_BEHAVIOR_DEFAULT:
        {
            result = SHADERS_VERTEX_UBER_TYPE_DEFAULT;

            break;
        }

        case MESH_MATERIAL_VS_BEHAVIOR_DUAL_PARABOLOID_SM:
        {
            result = SHADERS_VERTEX_UBER_TYPE_DUAL_PARABOLOID_SM;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized material_vs_behavior argument value");
        }
    } /* switch (material_vs_behavior) */

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t shaders_vertex_uber_get_n_items(__in __notnull shaders_vertex_uber shader)
{
    return system_resizable_vector_get_amount_of_elements(((_shaders_vertex_uber*)shader)->added_items);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader shaders_vertex_uber_get_shader(__in __notnull shaders_vertex_uber shader)
{
    _shaders_vertex_uber* shader_ptr = (_shaders_vertex_uber*) shader;

    if (shader_ptr->dirty)
    {
        LOG_INFO("Uber vertex shader marked as dirty - recompiling");

        shaders_vertex_uber_recompile(shader);
    }

    return shader_ptr->vertex_shader;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool shaders_vertex_uber_is_dirty(__in __notnull shaders_vertex_uber uber)
{
    return ((_shaders_vertex_uber*) uber)->dirty;
}

/* Please see header for specificationb */
PUBLIC EMERALD_API void shaders_vertex_uber_recompile(__in __notnull shaders_vertex_uber uber)
{
    _shaders_vertex_uber* uber_ptr = (_shaders_vertex_uber*) uber;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(uber_ptr->dirty,
                      "shaders_vertex_uber_recompile() failed for non-dirty object");

    /* Set the shader's body */
    system_hashed_ansi_string shader_body = ogl_shader_constructor_get_shader_body(uber_ptr->shader_constructor);
    bool                      result      = ogl_shader_set_body                   (uber_ptr->vertex_shader,
                                                                                   shader_body);

    ASSERT_DEBUG_SYNC(result,
                      "ogl_shader_set_body() failed");
    if (!result)
    {
        LOG_ERROR("Could not set uber vertex shader body.");

        goto end;
    }

    if (!ogl_shader_compile(uber_ptr->vertex_shader) )
    {
        LOG_ERROR("Could not compile uber vertex shader body.");

        goto end;
    }

    /* All done */
    uber_ptr->dirty = false;

end:
    ;
}