/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "shaders/shaders_fragment_texture2D_reinhard.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */
const char* tex2D_fragment_reinhardt_shader_body_not_reverted = "#version 330\n"
                                                                "\n"
                                                                "in vec2 uv;\n"
                                                                "\n"
                                                                "uniform data\n"
                                                                "{\n"
                                                                "    float exposure;\n"
                                                                "};\n"
                                                                "\n"
                                                                "uniform sampler2D tex;\n"
                                                                "\n"
                                                                "out vec4 result;\n"
                                                                "\n"
                                                                "void main()\n"
                                                                "{\n"
                                                                "    vec3 tex = texture(tex, uv).xyz * vec3(exposure);\n"
                                                                "\n"
                                                                "    result = vec4(pow(tex / (1 + tex), vec3(1/2.2) ), 1);\n"
                                                                "}\n";

const char* tex2D_fragment_reinhardt_shader_body_reverted = "#version 330\n"
                                                            "\n"
                                                            "in vec2 uv;\n"
                                                            "\n"
                                                            "uniform data\n"
                                                            "{\n"
                                                            "    float exposure;\n"
                                                            "};\n"
                                                            "\n"
                                                            "uniform sampler2D tex;\n"
                                                            "\n"
                                                            "out vec4 result;\n"
                                                            "\n"
                                                            "void main()\n"
                                                            "{\n"
                                                            "    vec3 tex = texture(tex, vec2(uv.x, 1-uv.y)).xyz * vec3(exposure);\n"
                                                            "\n"
                                                            "    result = vec4(pow(tex / (1 + tex), vec3(1/2.2) ), 1);\n"
                                                            "}\n";

/** Internal type definition */
typedef struct
{
    ogl_shader shader;

    REFCOUNT_INSERT_VARIABLES
} _shaders_fragment_texture2D_reinhardt;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(shaders_fragment_texture2D_reinhardt,
                               shaders_fragment_texture2D_reinhardt,
                               _shaders_fragment_texture2D_reinhardt);


/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_texture2D_reinhardt instance.
 **/
PRIVATE void _shaders_fragment_texture2D_reinhardt_release(__in __notnull __deallocate(mem) void* ptr)
{
    _shaders_fragment_texture2D_reinhardt* data_ptr = (_shaders_fragment_texture2D_reinhardt*) ptr;

    if (data_ptr->shader != NULL)
    {
        ogl_shader_release(data_ptr->shader);

        data_ptr->shader = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API shaders_fragment_texture2D_reinhardt shaders_fragment_texture2D_reinhardt_create(__in __notnull ogl_context               context,
                                                                                                    __in           bool                      should_revert_y,
                                                                                                    __in __notnull system_hashed_ansi_string name)
{
    _shaders_fragment_texture2D_reinhardt* result_object = NULL;
    shaders_fragment_texture2D_reinhardt   result_shader = NULL;

    /* Create the shader */
    ogl_shader shader = ogl_shader_create(context,
                                          SHADER_TYPE_FRAGMENT,
                                          name);

    ASSERT_DEBUG_SYNC(shader != NULL,
                      "Could not create a fragment shader.");

    if (shader == NULL)
    {
        LOG_ERROR("Could not create a fragment shader for Reinhardt Texture2D shader object.");

        goto end;
    }

    /* Attach body to the shader */
    if (!ogl_shader_set_body(shader,
                             system_hashed_ansi_string_create(should_revert_y ? tex2D_fragment_reinhardt_shader_body_reverted
                                                                              : tex2D_fragment_reinhardt_shader_body_not_reverted) ))
    {
        LOG_ERROR("Could not set body of Reinhardt Texture2D fragment shader.");

        ASSERT_DEBUG_SYNC(false,
                          "");

        goto end;
    }
    
    /* Everything went okay. Instantiate the object */
    result_object = new (std::nothrow) _shaders_fragment_texture2D_reinhardt;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _shaders_fragment_texture2D_reinhardt object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Reinhardt Texture2D object instance.");

        goto end;
    }

    result_object->shader = shader;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _shaders_fragment_texture2D_reinhardt_release,
                                                   OBJECT_TYPE_SHADERS_FRAGMENT_TEXTURE2D_REINHARDT,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Texture2D Reinhardt Fragment Shaders\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (shaders_fragment_texture2D_reinhardt) result_object;

end:
    if (shader != NULL)
    {
        ogl_shader_release(shader);

        shader = NULL;
    }

    if (result_object != NULL)
    {
        delete result_object;

        result_object = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader shaders_fragment_texture2D_reinhardt_get_shader(__in __notnull shaders_fragment_texture2D_reinhardt shader)
{
    return (((_shaders_fragment_texture2D_reinhardt*)shader)->shader);
}
