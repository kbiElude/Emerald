/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "mesh/mesh_material.h"
#include "glsl/glsl_shader_constructor.h"
#include "ogl/ogl_context.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "scene/scene_light.h"
#include "scene_renderer/scene_renderer_sm.h"
#include "shaders/shaders_fragment_uber.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>

/** Internal type definition */
typedef struct _shaders_fragment_uber_item_input_attribute
{
    shaders_fragment_uber_input_attribute_type attribute_type;
    shaders_fragment_uber_item_id              n_light;

    _shaders_fragment_uber_item_input_attribute()
    {
        attribute_type = UBER_INPUT_ATTRIBUTE_UNKNOWN;
        n_light        = -1;
    }
} _shaders_fragment_uber_item_input_attribute;

typedef struct _shaders_fragment_uber_item_light
{
    shaders_fragment_uber_light_type     light_type;
    shaders_fragment_uber_property_value properties[SHADERS_FRAGMENT_UBER_PROPERTY_COUNT];

    _shaders_fragment_uber_item_light()
    {
        light_type = SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE;

        /* Set default diffuse property values */
        properties[SHADERS_FRAGMENT_UBER_PROPERTY_AMBIENT_DATA_SOURCE]    = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE;
        properties[SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE]    = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE;
        properties[SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE] = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE;
        properties[SHADERS_FRAGMENT_UBER_PROPERTY_SHININESS_DATA_SOURCE]  = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE;
        properties[SHADERS_FRAGMENT_UBER_PROPERTY_SPECULAR_DATA_SOURCE]   = SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE;
    }

} _shaders_fragment_uber_item_light;

typedef struct _shaders_fragment_uber_item
{
    void*                           data;
    shaders_fragment_uber_item_type type;
} _shaders_fragment_uber_item;

typedef struct
{
    ral_context context;
    ral_shader  shader;

    system_resizable_vector                  added_items;
    bool                                     dirty;
    glsl_shader_constructor_uniform_block_id fragment_shader_properties_ub;
    glsl_shader_constructor                  shader_constructor;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_uber;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_uber,
                               shaders_fragment_uber,
                              _shaders_fragment_uber);


/* Internal variables */

/** TODO */
PRIVATE void _shaders_fragment_uber_add_lambert_ambient_diffuse_factor(scene_light                              light_instance,
                                                                       shaders_fragment_uber_light_type         light_type,
                                                                       glsl_shader_constructor                  shader_constructor,
                                                                       unsigned int                             n_item,
                                                                       shaders_fragment_uber_property_value*    properties,
                                                                       PFNSHADERSFRAGMENTUBERPARENTCALLBACKPROC pCallbackProc,
                                                                       void*                                    user_arg,
                                                                       system_hashed_ansi_string                light_visibility_var_name,
                                                                       glsl_shader_constructor_uniform_block_id fs_props_ub_id)
{
    scene_light_falloff light_falloff_type = SCENE_LIGHT_FALLOFF_UNKNOWN;

    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT   ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)
    {
        scene_light_get_property(light_instance,
                                 SCENE_LIGHT_PROPERTY_FALLOFF,
                                &light_falloff_type);
    }

    /* If we should take attenuation into consideration, calculate it at this point. */
    std::stringstream light_attenuation_var_name_sstream;
    std::stringstream light_attenuations_var_name_sstream;
    std::stringstream light_cone_angle_var_name_sstream;
    std::stringstream light_diffuse_var_name_sstream;
    std::stringstream light_direction_var_name_sstream;
    std::stringstream light_distance_var_name_sstream;
    std::stringstream light_edge_angle_var_name_sstream;
    std::stringstream light_range_var_name_sstream;
    std::stringstream light_spotlight_effect_var_name_sstream;
    std::stringstream light_vector_norm_var_name_sstream;
    std::stringstream line;
    bool              uses_attenuation      = false;
    bool              uses_spotlight_effect = false;

    light_attenuation_var_name_sstream      << "light"
                                            << n_item
                                            << "_attenuation";
    light_attenuations_var_name_sstream     << "light"
                                            << n_item
                                            << "_attenuations";
    light_cone_angle_var_name_sstream       << "light"
                                            << n_item
                                            << "_cone_angle";
    light_diffuse_var_name_sstream          << "light"
                                            << n_item
                                            << "_diffuse";
    light_direction_var_name_sstream        << "light"
                                            << n_item
                                            << "_direction";
    light_distance_var_name_sstream         << "light"
                                            << n_item
                                            << "_distance";
    light_edge_angle_var_name_sstream       << "light"
                                            << n_item
                                            << "_edge_angle";
    light_range_var_name_sstream            << "light"
                                            << n_item
                                            << "_range";
    light_spotlight_effect_var_name_sstream << "light"
                                            << n_item
                                            << "_spotlight_effect";
    light_vector_norm_var_name_sstream      << "light"
                                            << n_item
                                            << "_vector_norm";

    line << "\n// Lambert: ambient+diffuse (light:[" << n_item << "])\n";

    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)
    {
        std::stringstream cone_edge_diff_sstream;

        cone_edge_diff_sstream << "("
                               << light_cone_angle_var_name_sstream.str()
                               << " - "
                               << light_edge_angle_var_name_sstream.str()
                               << ")";

        glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                           VARIABLE_TYPE_UNIFORM,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                           0, /* array_size */
                                                           fs_props_ub_id,
                                                           system_hashed_ansi_string_create(light_cone_angle_var_name_sstream.str().c_str() ),
                                                           nullptr /* out_variable_id */);
        glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                           VARIABLE_TYPE_UNIFORM,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                           0, /* array_size */
                                                           fs_props_ub_id,
                                                           system_hashed_ansi_string_create(light_edge_angle_var_name_sstream.str().c_str() ),
                                                           nullptr /* out_variable_id */);

        line << "float "
             << light_spotlight_effect_var_name_sstream.str()
             << " = 0.0;\n"
             << "float "
             << light_spotlight_effect_var_name_sstream.str()
             << "_temp = dot(-"
             << light_direction_var_name_sstream.str()
             << ", "
             << light_vector_norm_var_name_sstream.str()
             << ");\n"
             << "float "
             << light_spotlight_effect_var_name_sstream.str()
             << "_temp_acos = acos("
             << light_spotlight_effect_var_name_sstream.str()
             << "_temp);\n"
             << "\n"
             << "if ("
             << light_spotlight_effect_var_name_sstream.str()
             << "_temp_acos > " << cone_edge_diff_sstream.str() << " && " /* cone - edge */
             << light_spotlight_effect_var_name_sstream.str()
             << "_temp_acos < " << light_cone_angle_var_name_sstream.str() << ")" /* cone */
             << light_spotlight_effect_var_name_sstream.str()
             << " = smoothstep(1.0, 0.0, (light0_spotlight_effect_temp_acos - " << cone_edge_diff_sstream.str() << ") / " << light_edge_angle_var_name_sstream.str() << ");\n" /* temp - (cone - edge), edge */
             << "else\n"
                "if ("
             << light_spotlight_effect_var_name_sstream.str()
             << "_temp_acos < " << light_cone_angle_var_name_sstream.str() << ")  " /* cone */
             << light_spotlight_effect_var_name_sstream.str()
             << " = "
             << light_spotlight_effect_var_name_sstream.str()
             << "_temp;\n";

        uses_spotlight_effect = true;
    }

    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT   ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)
    {
        line << "float "
             << light_distance_var_name_sstream.str()
             << " = length(light" << n_item << "_vector_non_norm);\n";

        uses_attenuation = true;

        switch (light_falloff_type)
        {
            case SCENE_LIGHT_FALLOFF_CUSTOM:
            {
                glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                                  VARIABLE_TYPE_UNIFORM,
                                                                  LAYOUT_QUALIFIER_NONE,
                                                                  RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                                  0, /* array_size */
                                                                  fs_props_ub_id,
                                                                  system_hashed_ansi_string_create(light_attenuations_var_name_sstream.str().c_str() ),
                                                                  nullptr /* out_variable_id */);

                line << "float "
                     << light_attenuation_var_name_sstream.str() << " = "
                     << ((uses_spotlight_effect) ? light_spotlight_effect_var_name_sstream.str() : "1.0")
                     << " / (light" << n_item << "_attenuations.x + "
                            "light" << n_item << "_attenuations.y * " << light_distance_var_name_sstream.str() << " + "
                            "light" << n_item << "_attenuations.z * " << light_distance_var_name_sstream.str() << " * " << light_distance_var_name_sstream.str() << ");\n";

                break;
            }

            case SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE:
            {
                glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                                   VARIABLE_TYPE_UNIFORM,
                                                                   LAYOUT_QUALIFIER_NONE,
                                                                   RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                                   0, /* array_size */
                                                                   fs_props_ub_id,
                                                                   system_hashed_ansi_string_create(light_range_var_name_sstream.str().c_str() ),
                                                                   nullptr /* out_variable_id */);

                line << "float "
                     << light_attenuation_var_name_sstream.str() << " = ";

                if (uses_spotlight_effect)
                {
                    line << light_spotlight_effect_var_name_sstream.str()
                         << " * ";
                }

                line << light_range_var_name_sstream.str()
                     << " / "
                     << light_distance_var_name_sstream.str()
                     << ";\n";

                 break;
            }

            case SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE_SQUARE:
            {
                glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                                   VARIABLE_TYPE_UNIFORM,
                                                                   LAYOUT_QUALIFIER_NONE,
                                                                   RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                                   0, /* array_size */
                                                                   fs_props_ub_id,
                                                                   system_hashed_ansi_string_create(light_range_var_name_sstream.str().c_str() ),
                                                                   nullptr /* out_variable_id */);

                line << "float "
                     << light_attenuation_var_name_sstream.str() << " = ";

                if (uses_spotlight_effect)
                {
                    line << light_spotlight_effect_var_name_sstream.str()
                         << " * ";
                }

                line << light_range_var_name_sstream.str()
                     << " / ("
                     << light_distance_var_name_sstream.str()
                     << " * "
                     << light_distance_var_name_sstream.str()
                     << ");\n";

                 break;
            }

            case SCENE_LIGHT_FALLOFF_LINEAR:
            {
                glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                                   VARIABLE_TYPE_UNIFORM,
                                                                   LAYOUT_QUALIFIER_NONE,
                                                                   RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                                   0, /* array_size */
                                                                   fs_props_ub_id,
                                                                   system_hashed_ansi_string_create(light_range_var_name_sstream.str().c_str() ),
                                                                   nullptr /* out_variable_id */);

                line << "float "
                     << light_attenuation_var_name_sstream.str()
                     << " = ";

                if (uses_spotlight_effect)
                {
                    line << light_spotlight_effect_var_name_sstream.str()
                         << " * ";
                }

                line << "clamp(1.0 - " << light_distance_var_name_sstream.str()
                     << " / "
                     << light_range_var_name_sstream.str()
                     << ", 0.0, 1.0);\n";

                 break;
            }

            case SCENE_LIGHT_FALLOFF_OFF:
            {
                line << "float "
                     << light_attenuation_var_name_sstream.str()
                     << " = ";

                if (uses_spotlight_effect)
                {
                    line << light_spotlight_effect_var_name_sstream.str()
                         << ";\n";
                }
                else
                {
                    line << "1.0;\n";
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized light falloff requested");
            }
        }
    }

    /* Compute diffuse factor for non-ambient lights */
    const bool is_luminosity_float_defined   = (properties[SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE] == SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_FLOAT                 ||
                                                properties[SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE] == SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_FLOAT);
    const bool is_luminosity_texture_defined = (properties[SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE] == SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_TEXTURE2D);

    line << "result_fragment += ";

    if (light_visibility_var_name != nullptr                                  &&
        light_type                != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT)
    {
        line << "vec4("
             << system_hashed_ansi_string_get_buffer(light_visibility_var_name)
             << ") * ";
    }

    if (uses_attenuation)
    {
        line << "vec4(" << light_attenuation_var_name_sstream.str() << ") * ";
    }

    line << "(";

    if (light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT)
    {
        if (is_luminosity_float_defined)
        {
            line << "vec4(vec3(luminosity_material), 1.0) ";
        }
        else
        if (is_luminosity_texture_defined)
        {
            line << "vec4(vec3(texture(luminosity_material_sampler, in_fs_uv).x), 1.0) ";
        }
        else
        {
            /* No luminosity defined for the material */
            line << "vec4(0.0)";
        }

        /* Compute the light contribution: diffuse */
        if (properties[SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE] != SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE)
        {
            line << "+ vec4(light" << n_item << "_LdotN_clamped)";
        }
    }
    else
    {
        switch (properties[SHADERS_FRAGMENT_UBER_PROPERTY_AMBIENT_DATA_SOURCE])
        {
            case SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE:
            {
                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "TODO: Add support for ambient materials");
            }
        }

        line << "vec4(ambient_color, 1.0)";
    }

    line << ")";

    if (light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT)
    {
        switch (properties[SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE])
        {
            case SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE:
            {
                break;
            }

            case SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_TEXTURE2D:
            {
                line << " * texture(diffuse_material_sampler, in_fs_uv)";

                break;
            }

            case SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_VEC3:
            {
                line << " * vec4(diffuse_material, 1.0)";

                break;
            }

            case SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_VEC4:
            {
                line << " * diffuse_material";

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized emission data source property value");
            }
        }

        /* Finally, multiply the result color with the diffuse color defined for the light */
        line << " * "
             << light_diffuse_var_name_sstream.str();
    }

    line << ";\n";

    /* Add the snippet to main function body */
    glsl_shader_constructor_append_to_function_body(shader_constructor,
                                                    0, /* main() */
                                                    system_hashed_ansi_string_create(line.str().c_str() ));
}

/** TODO */
PRIVATE void _shaders_fragment_uber_add_phong_specular_factor(glsl_shader_constructor   shader_constructor,
                                                              unsigned int              n_light,
                                                              system_hashed_ansi_string light_visibility_var_name)
{
    /* Add a phong_specular() function (if not already added) */
    glsl_shader_constructor_function_id phong_specular_func_id = -1;

    if (glsl_shader_constructor_add_function(shader_constructor,
                                             system_hashed_ansi_string_create("phong_specular"),
                                             RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                             &phong_specular_func_id) )
    {
        /* Configure the argument list */
        glsl_shader_constructor_add_function_argument(shader_constructor,
                                                      phong_specular_func_id,
                                                      SHADER_ARGUMENT_QUALIFIER_IN,
                                                      RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                      system_hashed_ansi_string_create("normal") );

        glsl_shader_constructor_add_function_argument(shader_constructor,
                                                      phong_specular_func_id,
                                                      SHADER_ARGUMENT_QUALIFIER_IN,
                                                      RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                      system_hashed_ansi_string_create("light_vector") );
        glsl_shader_constructor_add_function_argument(shader_constructor,
                                                      phong_specular_func_id,
                                                      SHADER_ARGUMENT_QUALIFIER_IN,
                                                      RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
                                                      system_hashed_ansi_string_create("light_LdotN") );

        /* Configure the body.
         *
         * NOTE: We assume here that glosiness & specular material properties are always represented by a float.
         *
         * TODO: Expand if needed!
         **/
        glsl_shader_constructor_set_function_body(shader_constructor,
                                                  phong_specular_func_id,
                                                  system_hashed_ansi_string_create("    if (light_LdotN > 0.0 && shininess_material > 0.0)\n"
                                                                                   "    {\n"
                                                                                   "        float reflection_view_vector_dot = max(0.0, dot(-reflect(light_vector, normal), normalize(view_vector) ));\n"
                                                                                   "\n"
                                                                                   "        return clamp(specular_material * pow(reflection_view_vector_dot, 32.0 * shininess_material), 0.0, 1.0);\n"
                                                                                   "    }\n"
                                                                                   "\n"
                                                                                   "    return 0.0;\n") );
    }

    /* Compute the light contribution */
    std::stringstream line;

    line << "\n// Phong: specular (light:[" << n_light << "])\n"
            "result_fragment += ";

    if (light_visibility_var_name != nullptr)
    {
        line << "vec4("
             << system_hashed_ansi_string_get_buffer(light_visibility_var_name)
             << ") * ";
    }

    line << "vec4(vec3(phong_specular(normal, light"
         << n_light
         << "_vector_norm, light"
         << n_light
         << "_LdotN)), 0.0);\n";

    glsl_shader_constructor_append_to_function_body(shader_constructor,
                                                    0, /* main() */
                                                    system_hashed_ansi_string_create(line.str().c_str() ));
}

/** TODO */
PRIVATE void _shaders_fragment_uber_add_sh3_diffuse_factor(glsl_shader_constructor shader_constructor,
                                                           unsigned int            n_light)
{
    /* Add SH-specific input attribute */
    std::stringstream attribute_name_sstream;

    attribute_name_sstream << "light"
                           << n_light
                           << "_mesh_ambient_diffuse_sh3";

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create(attribute_name_sstream.str().c_str() ),
                                                       nullptr /* out_variable_id */);

    /* Compute the light contribution */
    std::stringstream line;

    line << "result_fragment += vec4(" << attribute_name_sstream.str() << ", 0.0);\n";

    glsl_shader_constructor_append_to_function_body(shader_constructor,
                                                    0, /* main() */
                                                    system_hashed_ansi_string_create(line.str().c_str() ));
}

/** TODO */
PRIVATE void _shaders_fragment_uber_add_sh4_diffuse_factor(glsl_shader_constructor shader_constructor,
                                                           unsigned int            n_light)
{
    /* Add SH-specific input attribute */
    std::stringstream attribute_name_sstream;

    attribute_name_sstream << "light"
                           << n_light
                           << "_mesh_ambient_diffuse_sh4";

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create(attribute_name_sstream.str().c_str() ),
                                                       nullptr /* out_variable_id */);

    /* Compute the light contribution */
    std::stringstream line;

    line << "result_fragment += vec4(" << attribute_name_sstream.str() << ", 0.0);\n";

    glsl_shader_constructor_append_to_function_body(shader_constructor,
                                                    0, /* main() */
                                                    system_hashed_ansi_string_create(line.str().c_str() ));
}

/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_uber instance.
 **/
PRIVATE void _shaders_fragment_uber_release(void* ptr)
{
    _shaders_fragment_uber* data_ptr = (_shaders_fragment_uber*) ptr;

    if (data_ptr->added_items != nullptr)
    {
        _shaders_fragment_uber_item* item_ptr = nullptr;

        while (system_resizable_vector_pop(data_ptr->added_items,
                                          &item_ptr) )
        {
            switch (item_ptr->type)
            {
                case SHADERS_FRAGMENT_UBER_ITEM_INPUT_ATTRIBUTE:
                {
                    /* data holds a single value, nothing to release */

                    break;
                }

                case SHADERS_FRAGMENT_UBER_ITEM_LIGHT:
                {
                    delete (_shaders_fragment_uber_item_light*) item_ptr->data;

                    item_ptr->data = nullptr;
                    break;
                }

                default:
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unrecognized item type");
                }
            }

            delete item_ptr;
            item_ptr = nullptr;
        }

        system_resizable_vector_release(data_ptr->added_items);
        data_ptr->added_items = nullptr;
    }

    if (data_ptr->shader != nullptr)
    {
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&data_ptr->shader) );

        data_ptr->shader = nullptr;
    }

    if (data_ptr->shader_constructor != nullptr)
    {
        glsl_shader_constructor_release(data_ptr->shader_constructor);

        data_ptr->shader_constructor = nullptr;
    }

}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_uber_item_id shaders_fragment_uber_add_input_attribute_contribution(shaders_fragment_uber                      uber,
                                                                                                        shaders_fragment_uber_input_attribute_type attribute_type,
                                                                                                        PFNSHADERSFRAGMENTUBERPARENTCALLBACKPROC   pCallbackProc,
                                                                                                        void*                                      user_arg)
{
    std::stringstream         body_sstream;
    system_hashed_ansi_string fs_attribute_name_has;
    std::stringstream         fs_attribute_name_sstream;
    ral_program_variable_type fs_attribute_type;
    unsigned int              n_items  = 0;
    _shaders_fragment_uber*   uber_ptr = (_shaders_fragment_uber*) uber;
    system_hashed_ansi_string vs_attribute_name_has;

    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_items);

    /* Spawn an attribute item descriptor */
    _shaders_fragment_uber_item* new_item_ptr = new (std::nothrow) _shaders_fragment_uber_item;

    if (new_item_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(new_item_ptr != nullptr,
                           "Out of memory");

        goto end;
    }

    new_item_ptr->data = (void*) attribute_type;
    new_item_ptr->type = SHADERS_FRAGMENT_UBER_ITEM_INPUT_ATTRIBUTE;

    /* Determine attribute properties */
    switch (attribute_type)
    {
        case UBER_INPUT_ATTRIBUTE_NORMAL:
        {
            fs_attribute_name_sstream << "normal";
            fs_attribute_type         = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3;
            vs_attribute_name_has     = system_hashed_ansi_string_create("object_normal");

            break;
        }

        case UBER_INPUT_ATTRIBUTE_TEXCOORD:
        {
            fs_attribute_name_sstream << "in_fs_uv";
            fs_attribute_type         = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2;
            vs_attribute_name_has     = system_hashed_ansi_string_create("object_uv");

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized input attribute type");
        }
    }

    fs_attribute_name_has = system_hashed_ansi_string_create(fs_attribute_name_sstream.str().c_str() );

    /* Add new input attribute, if necessary */
    glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       fs_attribute_type,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       fs_attribute_name_has,
                                                       nullptr /* out_variable_id */);

    /* Form new line */
    body_sstream << "result_fragment += vec4(";

    switch (fs_attribute_type)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2:
        {
            body_sstream << fs_attribute_name_sstream.str().c_str() << ", 0, 0);\n";

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3:
        {
            body_sstream << fs_attribute_name_sstream.str().c_str() << ", 0);\n";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized fs attribute type");
        }
    }

    /* Add it to the body */
    glsl_shader_constructor_append_to_function_body(uber_ptr->shader_constructor,
                                                    0, /* function_id */
                                                    system_hashed_ansi_string_create(body_sstream.str().c_str() ));

    /* Call back the parent so that vertex shader actually passes the data we need to fragment shader stage */
    if (pCallbackProc != nullptr)
    {
        _shaders_fragment_uber_new_fragment_input_callback callback_data;

        callback_data.fs_attribute_name = fs_attribute_name_has;
        callback_data.fs_attribute_type = fs_attribute_type;
        callback_data.vs_attribute_name = vs_attribute_name_has;

        pCallbackProc(SHADERS_FRAGMENT_UBER_PARENT_CALLBACK_NEW_FRAGMENT_INPUT,
                     &callback_data,
                      user_arg);
    }
    else
    {
        LOG_ERROR("shaders_fragment_uber missed a call-back - user-defined call-back func ptr is NULL");
    }

    /* Add the descriptor to added items vector */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    /* Reset the shader's body. This will prevent dependent programs from linking with outdated body. */
    ral_shader_set_property(uber_ptr->shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                            nullptr);

    uber_ptr->dirty = true;

end:
    return n_items;
}

/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_uber_item_id shaders_fragment_uber_add_light(shaders_fragment_uber                    uber,
                                                                                 shaders_fragment_uber_light_type         light_type,
                                                                                 scene_light                              light_instance,
                                                                                 bool                                     is_shadow_caster,
                                                                                 unsigned int                             n_light_properties,
                                                                                 void*                                    light_property_values,
                                                                                 PFNSHADERSFRAGMENTUBERPARENTCALLBACKPROC pCallbackProc,
                                                                                 void*                                    user_arg)
{
    _shaders_fragment_uber* uber_ptr = (_shaders_fragment_uber*) uber;
    unsigned int            n_items  = 0;

    system_resizable_vector_get_property(uber_ptr->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_items);

    /* Spawn a light item descriptor */
    _shaders_fragment_uber_item*       new_item_ptr            = new (std::nothrow) _shaders_fragment_uber_item;
    _shaders_fragment_uber_item_light* new_light_item_data_ptr = new (std::nothrow) _shaders_fragment_uber_item_light;

    ASSERT_ALWAYS_SYNC(new_item_ptr != nullptr,
                       "Out of memory");
    ASSERT_ALWAYS_SYNC(new_light_item_data_ptr != nullptr,
                       "Out of memory");

    new_light_item_data_ptr->light_type = light_type;

    new_item_ptr->data = new_light_item_data_ptr;
    new_item_ptr->type = SHADERS_FRAGMENT_UBER_ITEM_LIGHT;

    for (unsigned int n_property = 0;
                      n_property < n_light_properties;
                    ++n_property)
    {
        shaders_fragment_uber_property       property = ((shaders_fragment_uber_property*)       light_property_values)[n_property * 2 + 0];
        shaders_fragment_uber_property_value value    = ((shaders_fragment_uber_property_value*) light_property_values)[n_property * 2 + 1];

        new_light_item_data_ptr->properties[property] = value;
    }

    /* Add properties to shader-wide named uniform buffer */
    std::stringstream         light_attenuations_name_sstream;
    std::stringstream         light_diffuse_name_sstream;
    std::stringstream         light_direction_name_sstream;
    std::stringstream         light_vector_non_norm_name_sstream;
    std::stringstream         light_vector_norm_name_sstream;
    system_hashed_ansi_string light_visibility_var_name_has = nullptr;
    std::stringstream         light_world_pos_name_sstream;

    /* Add light properties */
    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT       ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL   ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT         ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)
    {
        light_diffuse_name_sstream << "light" << n_items << "_diffuse";

        glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                           VARIABLE_TYPE_UNIFORM,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                           0, /* array_size */
                                                           uber_ptr->fragment_shader_properties_ub,
                                                           system_hashed_ansi_string_create(light_diffuse_name_sstream.str().c_str() ),
                                                           nullptr /* out_variable_id */);
    }

    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT   ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)
    {
        light_world_pos_name_sstream << "light" << n_items << "_world_pos";

        glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                           VARIABLE_TYPE_UNIFORM,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                           0, /* array_size */
                                                           uber_ptr->fragment_shader_properties_ub,
                                                           system_hashed_ansi_string_create(light_world_pos_name_sstream.str().c_str() ),
                                                           nullptr /* out_variable_id */);
    }

    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL   ||
        light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT)
    {
        light_direction_name_sstream << "light" << n_items << "_direction";

        glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                           VARIABLE_TYPE_UNIFORM,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                           0, /* array_size */
                                                           uber_ptr->fragment_shader_properties_ub,
                                                           system_hashed_ansi_string_create(light_direction_name_sstream.str().c_str() ),
                                                           nullptr /* out_variable_id */);
    }

    /* Add light vector calculation */
    if (light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT)
    {
        std::stringstream line;

        light_vector_norm_name_sstream     << "light"
                                           << n_items
                                           << "_vector_norm";
        light_vector_non_norm_name_sstream << "light"
                                           << n_items
                                           << "_vector_non_norm";

        if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL ||
            light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL)
        {
            line << "vec3 "
                 << light_vector_non_norm_name_sstream.str()
                 << " = -"
                 << light_direction_name_sstream.str()
                 << ";\n"
                 << "vec3 "
                 << light_vector_norm_name_sstream.str()
                 << " = normalize("
                 << light_vector_non_norm_name_sstream.str()
                 << ");\n";
        }
        else
        {
            ASSERT_DEBUG_SYNC(light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT ||
                              light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT   ||
                              light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT,
                              "Unrecognized light type");

            line << "vec3 "
                 << light_vector_non_norm_name_sstream.str()
                 << " = "
                 << light_world_pos_name_sstream.str()
                 << ".xyz - world_vertex;\n"
                 << "vec3 "
                 << light_vector_norm_name_sstream.str()
                 << " = normalize("
                 << light_vector_non_norm_name_sstream.str()
                 << ");\n";
        }

        glsl_shader_constructor_append_to_function_body(uber_ptr->shader_constructor,
                                                        0, /* main() */
                                                        system_hashed_ansi_string_create(line.str().c_str() ));
    }

    /* Add ambient light uniform if needed. */
    if (light_type == SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT)
    {
        glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                           VARIABLE_TYPE_UNIFORM,
                                                           LAYOUT_QUALIFIER_NONE,
                                                           RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                           0, /* array_size */
                                                           uber_ptr->fragment_shader_properties_ub,
                                                           system_hashed_ansi_string_create("ambient_color"),
                                                           nullptr /* out_variable_id */);
    }

    /* Add material-specific input attributes & uniforms if not already defined. Note that all the names
     * are suffixed with light index.
     */
    struct _properties
    {
        shaders_fragment_uber_property       property;
        shaders_fragment_uber_property_value value;

        /* Item-specific */
        ral_program_variable_type shader_uniform_type;
        const char*               shader_uniform_name;

        /* Object material-specific */
        ral_program_variable_type uv_sampler_uniform_type;
        const char*               uv_sampler_uniform_name;
        const char*               uv_fs_input_attribute_name;
        const char*               uv_vs_input_attribute_name;
    }
    properties_data[] =
    {
        {
            SHADERS_FRAGMENT_UBER_PROPERTY_AMBIENT_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_TEXTURE2D,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
            "ambient_material",

            RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D,
            "ambient_material_sampler",
            "in_fs_uv",
            "object_uv"
        },

        {
            SHADERS_FRAGMENT_UBER_PROPERTY_AMBIENT_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_VEC3,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
            "ambient_material",

            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },
        {
            SHADERS_FRAGMENT_UBER_PROPERTY_AMBIENT_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_VEC4,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
            "ambient_material",

            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },

        {
            SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_TEXTURE2D,

            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED,
            nullptr,

            RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D,
            "diffuse_material_sampler",
            "in_fs_uv",
            "object_uv"
        },

        {
            SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_VEC3,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
            "diffuse_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },
        {
            SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_VEC4,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
            "diffuse_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },

        {
            SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_TEXTURE2D,

            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED,
            nullptr,

            RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D,
            "luminosity_material_sampler",
            "in_fs_uv",
            "object_uv"
        },

        {
            SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_FLOAT,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
            "luminosity_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },
        {
            SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_FLOAT,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
            "luminosity_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },

        {
            SHADERS_FRAGMENT_UBER_PROPERTY_SHININESS_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_FLOAT,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
            "shininess_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },
        {
            SHADERS_FRAGMENT_UBER_PROPERTY_SHININESS_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_FLOAT,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
            "shininess_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },

        {
            SHADERS_FRAGMENT_UBER_PROPERTY_SPECULAR_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_FLOAT,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
            "specular_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },
        {
            SHADERS_FRAGMENT_UBER_PROPERTY_SPECULAR_DATA_SOURCE,
            SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_FLOAT,

            RAL_PROGRAM_VARIABLE_TYPE_FLOAT,
            "specular_material",
            RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED
        },
    };
    const unsigned int n_properties = sizeof(properties_data) / sizeof(properties_data[0]);

    for (unsigned int n_property = 0;
                      n_property < n_properties;
                    ++n_property)
    {
        const _properties& iteration_data = properties_data[n_property];

        if (new_light_item_data_ptr->properties[iteration_data.property] == iteration_data.value)
        {
            if (iteration_data.uv_fs_input_attribute_name != nullptr)
            {
                ASSERT_DEBUG_SYNC(iteration_data.uv_vs_input_attribute_name != nullptr,
                                  "No VS input attribute name defined");

                /** Add a new attribute if necessary */
                system_hashed_ansi_string attribute_name_has = system_hashed_ansi_string_create(iteration_data.uv_fs_input_attribute_name);

                if (glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                                      VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                                      LAYOUT_QUALIFIER_NONE,
                                                                      RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2,
                                                                      0, /* array_size */
                                                                      0, /* uniform_block */
                                                                      attribute_name_has,
                                                                      nullptr /* out_variable_id */) )
                {
                    /* Since we're adding an input attribute, we must ask the parent to set up
                     * the attribute chain for us */
                    if (pCallbackProc != nullptr)
                    {
                        _shaders_fragment_uber_new_fragment_input_callback callback_data;

                        callback_data.fs_attribute_name = attribute_name_has;
                        callback_data.fs_attribute_type = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2;
                        callback_data.vs_attribute_name = system_hashed_ansi_string_create(iteration_data.uv_vs_input_attribute_name);

                        pCallbackProc(SHADERS_FRAGMENT_UBER_PARENT_CALLBACK_NEW_FRAGMENT_INPUT,
                                     &callback_data,
                                      user_arg);
                    }
                    else
                    {
                        LOG_ERROR("shaders_fragment_uber missed a call-back - user-defined call-back func ptr is NULL");
                    }
                }
            }

            if (iteration_data.shader_uniform_name != nullptr)
            {
                glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                                   VARIABLE_TYPE_UNIFORM,
                                                                   LAYOUT_QUALIFIER_NONE,
                                                                   iteration_data.shader_uniform_type,
                                                                   0, /* array_size */
                                                                   uber_ptr->fragment_shader_properties_ub,
                                                                   system_hashed_ansi_string_create(iteration_data.shader_uniform_name),
                                                                   nullptr /* out_variable_id */);
            }

            if (iteration_data.uv_sampler_uniform_name != nullptr)
            {
                glsl_shader_constructor_add_general_variable_to_ub(uber_ptr->shader_constructor,
                                                                   VARIABLE_TYPE_UNIFORM,
                                                                   LAYOUT_QUALIFIER_NONE,
                                                                   iteration_data.uv_sampler_uniform_type,
                                                                   0, /* array_size */
                                                                   0, /* uniform_block */
                                                                   system_hashed_ansi_string_create(iteration_data.uv_sampler_uniform_name),
                                                                   nullptr /* out_variable_id */);
            }
        }
    }

    /* Add a variable storing result of dot(normal, light). This is used by both shading &
     * shadow map calculations */
    if (light_type != SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT)
    {
        std::stringstream ldotn_sstream;

        ldotn_sstream << "float light" << n_items << "_LdotN "
                      << "= dot("
                      << "light" << n_items << "_vector_norm"
                      << ", normal);\n";
        ldotn_sstream << "float light"   << n_items << "_LdotN_clamped "
                      << "= clamp(light" << n_items << "_LdotN, 0.0, 1.0);\n";

        glsl_shader_constructor_append_to_function_body(uber_ptr->shader_constructor,
                                                        0, /* main() */
                                                        system_hashed_ansi_string_create(ldotn_sstream.str().c_str() ));
    }

    /* Add shadow map support */
    if (is_shadow_caster)
    {
        scene_renderer_sm_adjust_fragment_uber_code(uber_ptr->shader_constructor,
                                                    n_items,
                                                    light_instance,
                                                    uber_ptr->fragment_shader_properties_ub,
                                                    system_hashed_ansi_string_create(light_world_pos_name_sstream.str().c_str() ),
                                                    system_hashed_ansi_string_create(light_vector_norm_name_sstream.str().c_str() ),
                                                    system_hashed_ansi_string_create(light_vector_non_norm_name_sstream.str().c_str() ),
                                                   &light_visibility_var_name_has);
    }

    /* Compute ambient + diffuse light factors */
    switch (light_type)
    {
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT:
        {
            _shaders_fragment_uber_add_lambert_ambient_diffuse_factor(light_instance,
                                                                      light_type,
                                                                      uber_ptr->shader_constructor,
                                                                      n_items,
                                                                      new_light_item_data_ptr->properties,
                                                                      pCallbackProc,
                                                                      user_arg,
                                                                      light_visibility_var_name_has,
                                                                      uber_ptr->fragment_shader_properties_ub);

            break;
        }

        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PROJECTION_SH3:
        {
            _shaders_fragment_uber_add_sh3_diffuse_factor(uber_ptr->shader_constructor,
                                                          n_items);

            break;
        }

        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PROJECTION_SH4:
        {
            _shaders_fragment_uber_add_sh4_diffuse_factor(uber_ptr->shader_constructor,
                                                          n_items);

            break;
        }

        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE:
        {
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized diffuse light type");
        }
    }

    /* Compute specular light factor */
    switch (light_type)
    {
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT:
        case SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT:
        {
            _shaders_fragment_uber_add_phong_specular_factor(uber_ptr->shader_constructor,
                                                             n_items,
                                                             light_visibility_var_name_has);

            break;
        }
    }

    /* Add the descriptor to added items vector */
    system_resizable_vector_push(uber_ptr->added_items,
                                 new_item_ptr);

    /* Reset the shader's body. This will prevent dependent programs from linking with outdated body. */
    ral_shader_set_property(uber_ptr->shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                            nullptr);

    uber_ptr->dirty = true;

    return n_items;
}

/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_uber shaders_fragment_uber_create(ral_context                context,
                                                                      system_hashed_ansi_string  name)
{
    ral_shader                               embedded_shader               = nullptr;
    glsl_shader_constructor_uniform_block_id fragment_shader_properties_ub = 0;
    system_hashed_ansi_string                ral_shader_instance_name      = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                                     " fragment uber");
    _shaders_fragment_uber*                  result_object_ptr             = nullptr;
    shaders_fragment_uber                    result_shader                 = nullptr;
    glsl_shader_constructor                  shader_constructor            = nullptr;
    ral_shader_create_info                   shader_create_info;

    /* Create a new ral_shader instance only if one is not already registered */
    if ((ral_context_get_shader_by_name(context,
                                        ral_shader_instance_name)) != nullptr)
    {
        /* It is an error if a shader with the same name already exists. */
        ASSERT_DEBUG_SYNC(false,
                          "A RAL shader named [%s] already exists!",
                          system_hashed_ansi_string_get_buffer(ral_shader_instance_name) );

        goto end;
    }

    /* Initialize the shader constructor */
    shader_constructor = glsl_shader_constructor_create(RAL_SHADER_TYPE_FRAGMENT,
                                                        system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                                " fragment uber"));

    ASSERT_DEBUG_SYNC(shader_constructor != nullptr,
                      "Could not create a shader constructor");

    if (shader_constructor == nullptr)
    {
        LOG_ERROR("Could not create a shader constructor");

        goto end;
    }

    /* Add FragmentShaderProperties uniform block */
    fragment_shader_properties_ub = glsl_shader_constructor_add_uniform_block(shader_constructor,
                                                                              LAYOUT_QUALIFIER_STD140,
                                                                              system_hashed_ansi_string_create("FragmentShaderProperties") );

    /* Add input attributes */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("world_vertex"),
                                                       nullptr /* out_variable_id */);

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("out_vs_normal"),
                                                       nullptr /* out_variable_id */);

    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_INPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("view_vector"),
                                                       nullptr /* out_variable_id */);

    /* Add output attributes */
    glsl_shader_constructor_add_general_variable_to_ub(shader_constructor,
                                                       VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
                                                       LAYOUT_QUALIFIER_NONE,
                                                       RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4,
                                                       0, /* array_size */
                                                       0, /* uniform_block */
                                                       system_hashed_ansi_string_create("result_fragment"),
                                                       nullptr /* out_variable_id */);

    /* Set base shader body */
    glsl_shader_constructor_set_function_body(shader_constructor,
                                              0, /* function_id */
                                              system_hashed_ansi_string_create("vec3 normal     = normalize(out_vs_normal);\n"
                                                                               "result_fragment = vec4(0, 0, 0, 1);\n") );

    /* Spawn a new RAL shader instance */
    system_hashed_ansi_string shader_body = glsl_shader_constructor_get_shader_body(shader_constructor);

    shader_create_info.name   = ral_shader_instance_name;
    shader_create_info.source = RAL_SHADER_SOURCE_GLSL;
    shader_create_info.type   = RAL_SHADER_TYPE_FRAGMENT;

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &embedded_shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a new ral_shader instance for a uber fragment shader.");

        goto end;
    }

    ral_shader_set_property(embedded_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Everything went okay. Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_fragment_uber;

    ASSERT_DEBUG_SYNC(result_object_ptr != nullptr,
                      "Out of memory while instantiating _shaders_fragment_uber object.");

    if (result_object_ptr == nullptr)
    {
        LOG_ERROR("Out of memory while creating uber object instance.");

        goto end;
    }

    result_object_ptr->added_items                   = system_resizable_vector_create(4 /* capacity */);
    result_object_ptr->context                       = context;
    result_object_ptr->dirty                         = true;
    result_object_ptr->fragment_shader_properties_ub = fragment_shader_properties_ub;
    result_object_ptr->shader                        = embedded_shader;
    result_object_ptr->shader_constructor            = shader_constructor;

    ASSERT_ALWAYS_SYNC(result_object_ptr->added_items != nullptr,
                       "Out of memory");

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_fragment_uber_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_UBER,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Uber Fragment Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
end:
    return (shaders_fragment_uber) result_object_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool shaders_fragment_uber_get_item_type(shaders_fragment_uber            uber,
                                                            shaders_fragment_uber_item_id    item_id,
                                                            shaders_fragment_uber_item_type* out_item_type)
{
    _shaders_fragment_uber_item* item_ptr = nullptr;
    _shaders_fragment_uber*      uber_ptr = (_shaders_fragment_uber*) uber;
    bool                         result   = false;

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
PUBLIC EMERALD_API bool shaders_fragment_uber_get_light_item_properties(const shaders_fragment_uber       uber,
                                                                        shaders_fragment_uber_item_id     item_id,
                                                                        shaders_fragment_uber_light_type* out_light_type)
{
    bool                          result        = false;
    const _shaders_fragment_uber* uber_ptr      = (const _shaders_fragment_uber*) uber;
    _shaders_fragment_uber_item*  uber_item_ptr = nullptr;

    if (system_resizable_vector_get_element_at(uber_ptr->added_items,
                                               item_id,
                                              &uber_item_ptr) )
    {
        ASSERT_DEBUG_SYNC(uber_item_ptr->type == SHADERS_FRAGMENT_UBER_ITEM_LIGHT,
                          "Light item property getter operating on a non-light uber item");

        if (uber_item_ptr->type == SHADERS_FRAGMENT_UBER_ITEM_LIGHT)
        {
            *out_light_type = ((_shaders_fragment_uber_item_light*) uber_item_ptr->data)->light_type;
            result          = true;
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized uber item requested");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t shaders_fragment_uber_get_n_items(shaders_fragment_uber shader)
{
    uint32_t result = 0;

    system_resizable_vector_get_property(((_shaders_fragment_uber*)shader)->added_items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_fragment_uber_get_shader(shaders_fragment_uber shader)
{
    _shaders_fragment_uber* shader_ptr = (_shaders_fragment_uber*) shader;

    if (shader_ptr->dirty)
    {
        LOG_INFO("Uber fragment shader marked as dirty - recompiling");

        shaders_fragment_uber_recompile(shader);
    }

    return shader_ptr->shader;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool shaders_fragment_uber_is_dirty(shaders_fragment_uber uber)
{
    return ((_shaders_fragment_uber*) uber)->dirty;
}

/** Please see header for specification */
PUBLIC EMERALD_API void shaders_fragment_uber_recompile(shaders_fragment_uber uber)
{
    _shaders_fragment_uber* uber_ptr = (_shaders_fragment_uber*) uber;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(uber_ptr->dirty,
                      "shaders_fragment_uber_recompile() failed for non-dirty object");

    /* Set the shader's body. The back-end will decide whether it makes sense to compile the shader now
     * or later. **/
    system_hashed_ansi_string shader_body = glsl_shader_constructor_get_shader_body(uber_ptr->shader_constructor);

    ral_shader_set_property(uber_ptr->shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* All done */
    uber_ptr->dirty = false;
}
