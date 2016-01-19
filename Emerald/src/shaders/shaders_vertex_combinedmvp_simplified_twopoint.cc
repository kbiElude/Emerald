/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_vertex_combinedmvp_simplified_twopoint.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/** Internal type defintiion */
typedef struct
{
    ral_context context;
    ral_shader  shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_vertex_combinedmvp_simplified_twopoint;

static const char* shader_body = "#version 430 core\n"
                                 "\n"
                                 "in vec2 in_uv;\n"
                                 "\n"
                                 "uniform dataVS\n"
                                 "{\n"
                                 "    vec4 a;\n"
                                 "    vec4 b;\n"
                                 "    mat4 mvp;\n"
                                 "};\n"
                                 "\n"
                                 "out vec2 uv;\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "   gl_Position = mvp * (gl_VertexID == 0 ? a : b);\n"
                                 "   uv          = in_uv;\n"
                                 "}\n";

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_vertex_combinedmvp_simplified_twopoint,
                               shaders_vertex_combinedmvp_simplified_twopoint,
                              _shaders_vertex_combinedmvp_simplified_twopoint);


/** Function called back when reference counter drops to zero. Releases the shader object.
 *
 *  @param ptr Pointer to _shaders_vertex_combinedmvp instance.
 **/
PRIVATE void _shaders_vertex_combinedmvp_simplified_twopoint_release(void* ptr)
{
    _shaders_vertex_combinedmvp_simplified_twopoint* data_ptr = (_shaders_vertex_combinedmvp_simplified_twopoint*) ptr;

    if (data_ptr->shader != NULL)
    {
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                  &data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_vertex_combinedmvp_simplified_twopoint shaders_vertex_combinedmvp_simplified_twopoint_create(ral_context               context,
                                                                                                                        system_hashed_ansi_string name)
{
    bool                                             result            = false;
    _shaders_vertex_combinedmvp_simplified_twopoint* result_object_ptr = NULL;
    ral_shader                                       shader            = NULL;

    /* Create the shader */
    system_hashed_ansi_string shader_body       (system_hashed_ansi_string_create(shader_body) );
    ral_shader_create_info    shader_create_info(name,
                                                 RAL_SHADER_TYPE_VERTEX);

    if (!ral_context_create_shaders(context,
                                    1, /* n_create_info_items */
                                   &shader_create_info,
                                   &shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a new RAL shader instance.");

        goto end;
    }

    ral_shader_set_property(shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    /* Instantiate the object */
    result_object_ptr = new (std::nothrow) _shaders_vertex_combinedmvp_simplified_twopoint;

    if (result_object_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(result_object_ptr != NULL,
                          "Out of memory while instantiating _shaders_vertex_combinedmvp_simplified_twopoint object.");

        goto end;
    }

    result_object_ptr->context = context;
    result_object_ptr->shader  = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object_ptr,
                                                   _shaders_vertex_combinedmvp_simplified_twopoint_release,
                                                   OBJECT_TYPE_SHADERS_VERTEX_COMBINEDMVP_SIMPLIFIED_TWO_POINT,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Combined MVP Simplified 2-point Vertex Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_vertex_combinedmvp_simplified_twopoint) result_object_ptr;

end:
    if (shader != NULL)
    {
        ral_context_delete_objects(context,
                                   RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                   1, /* n_objects */
                                  &shader);
    }

    if (result_object_ptr != NULL)
    {
        delete result_object_ptr;

        result_object_ptr = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_shader shaders_vertex_combinedmvp_simplified_twopoint_get_shader(shaders_vertex_combinedmvp_simplified_twopoint shader)
{
    return ((_shaders_vertex_combinedmvp_simplified_twopoint*)shader)->shader;
}