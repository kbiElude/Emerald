/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_shaders.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{
    system_hashed_ansi_string body;
    bool                      compile_status;
    ogl_context               context; /* DO NOT retain - context owns the instance */
    bool                      has_been_compiled;
    GLuint                    id;
    system_hashed_ansi_string name;
    char*                     shader_info_log;
    ogl_shader_type           type;

    /* GL entry-point cache */
    PFNGLCOMPILESHADERPROC    pGLCompileShader;
    PFNGLCREATESHADERPROC     pGLCreateShader;
    PFNGLDELETESHADERPROC     pGLDeleteShader;
    PFNGLGETERRORPROC         pGLGetError;
    PFNGLGETSHADERINFOLOGPROC pGLGetShaderInfoLog;
    PFNGLGETSHADERIVPROC      pGLGetShaderiv;
    PFNGLSHADERSOURCEPROC     pGLShaderSource;
    REFCOUNT_INSERT_VARIABLES
} _ogl_shader;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_shader,
                               ogl_shader,
                              _ogl_shader);


/** TODO */
PRIVATE void _ogl_shader_compile_callback(__in __notnull ogl_context context,
                                                         void*       in_arg)
{
    _ogl_shader* shader_ptr = (_ogl_shader*) in_arg;

    shader_ptr->pGLCompileShader(shader_ptr->id);

    /* Make sure everything went as expected */
    ASSERT_DEBUG_SYNC(shader_ptr->pGLGetError() == GL_NO_ERROR,
                      "Error occurred while compiling shader");

    /* Retrieve compile status and shader info log */
    GLint   compile_status_result  = 0;
    GLsizei shader_info_log_length = 0;

    shader_ptr->pGLGetShaderiv(shader_ptr->id,
                               GL_COMPILE_STATUS,
                              &compile_status_result);
    shader_ptr->pGLGetShaderiv(shader_ptr->id,
                               GL_INFO_LOG_LENGTH,
                              &shader_info_log_length);

    shader_ptr->compile_status = (compile_status_result == 1);

    if (shader_ptr->shader_info_log != NULL)
    {
        delete shader_ptr->shader_info_log;
    }

    if (shader_info_log_length != 0)
    {
        shader_ptr->shader_info_log = new (std::nothrow) char[shader_info_log_length + 1];

        ASSERT_ALWAYS_SYNC(shader_ptr->shader_info_log != NULL,
                           "Out of memory while allocating space for shader info log [%d bytes]",
                           shader_info_log_length+1);

        if (shader_ptr->shader_info_log != NULL)
        {
            memset(shader_ptr->shader_info_log,
                   0,
                   shader_info_log_length + 1);

            shader_ptr->pGLGetShaderInfoLog(shader_ptr->id,
                                            shader_info_log_length + 1,
                                            NULL,
                                            shader_ptr->shader_info_log);

            LOG_ERROR("Shader info log for shader [%d] follows: \n>>\n%s\n<<",
                      shader_ptr->id,
                      shader_ptr->shader_info_log);

            ASSERT_DEBUG_SYNC(shader_ptr->pGLGetError() == GL_NO_ERROR,
                              "Error occurred while retrieving compile status or shader info log.");
        }
    }
}

/** TODO */
PRIVATE void _ogl_shader_create_callback(__in __notnull ogl_context context, void* in_arg)
{
    _ogl_shader* shader_ptr = (_ogl_shader*) in_arg;

    shader_ptr->id = shader_ptr->pGLCreateShader(shader_ptr->type);
}

/** TODO */
PRIVATE void _ogl_shader_release_callback(__in __notnull ogl_context context, void* in_arg)
{
    _ogl_shader* shader_ptr = (_ogl_shader*) in_arg;

    shader_ptr->pGLDeleteShader(shader_ptr->id);

    ASSERT_DEBUG_SYNC(shader_ptr->pGLGetError() == GL_NO_ERROR,
                      "Error occurred while deleting a shader.");
}

/** TODO */
PRIVATE void _ogl_shader_set_body_callback(__in __notnull ogl_context context, void* in_arg)
{
    _ogl_shader* shader_ptr           = (_ogl_shader*) in_arg;
    const char*  shader_source        = system_hashed_ansi_string_get_buffer(shader_ptr->body);
    uint32_t     shader_source_length = system_hashed_ansi_string_get_length(shader_ptr->body);

    shader_ptr->pGLShaderSource(shader_ptr->id,
                                1,
                                (const GLchar**) &shader_source,
                                (const GLint*)   &shader_source_length
                               );

    ASSERT_DEBUG_SYNC(shader_ptr->pGLGetError() == GL_NO_ERROR,
                      "Error occurred while setting shader source.");
}

/** TODO */
PRIVATE void _ogl_shader_release(__in __notnull __deallocate(mem) void* shader)
{
    _ogl_shader* shader_ptr = (_ogl_shader*) shader;

    if (shader_ptr->shader_info_log != NULL)
    {
        delete shader_ptr->shader_info_log;
        shader_ptr->shader_info_log = NULL;
    }

    ogl_context_request_callback_from_context_thread(shader_ptr->context,
                                                     _ogl_shader_release_callback,
                                                     shader_ptr);

    /* Finally, unregister the shader from the shaders manager. */
    ogl_shaders shaders = NULL;

    ogl_context_get_property(shader_ptr->context,
                             OGL_CONTEXT_PROPERTY_SHADERS,
                            &shaders);

    if (shaders != NULL)
    {
        ogl_shaders_unregister_shader(shaders,
                                      (ogl_shader) shader_ptr);
    }
    else
    {
        LOG_FATAL("Shader manager is NULL. An ogl_shader instance will not be unregistered.")
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_compile(__in __notnull ogl_shader shader)
{
    bool         result     = false;
    _ogl_shader* shader_ptr = (_ogl_shader*) shader;
    
    if (system_hashed_ansi_string_get_length(shader_ptr->body) != 0)
    {
        ogl_context_request_callback_from_context_thread(shader_ptr->context,
                                                         _ogl_shader_compile_callback,
                                                         shader_ptr);

        result                        = shader_ptr->compile_status;
        shader_ptr->has_been_compiled = true;
    }
    else
    {
        LOG_ERROR("Cannot compile - no body assigned to the shader.");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader ogl_shader_create(__in __notnull ogl_context               context,
                                                __in           ogl_shader_type           shader_type,
                                                __in __notnull system_hashed_ansi_string name)
{
    /* Sanity check: make sure the ogl_shader instance we're about to create has not already been created.
     *               If you ever reach this point, please ensure shader instances are uniquely named. */
    ogl_shaders shaders = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SHADERS,
                            &shaders);

    ASSERT_DEBUG_SYNC(ogl_shaders_get_shader_by_name(shaders,
                                                     name) == NULL,
                      "Shader [%s] is already registered!",
                      system_hashed_ansi_string_get_buffer(name) );

    /* Carry on and create a new shader instance */
    _ogl_shader* result = new (std::nothrow) _ogl_shader;

    if (result != NULL)
    {
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                       _ogl_shader_release,
                                                       OBJECT_TYPE_OGL_SHADER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Shaders\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        result->body              = system_hashed_ansi_string_get_default_empty_string();
        result->compile_status    = false;
        result->context           = context;
        result->has_been_compiled = false;
        result->id                = 0;
        result->name              = name;
        result->shader_info_log   = NULL;
        result->type              = shader_type;

        /* Initialize entry-point cache */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            result->pGLCompileShader    = entry_points->pGLCompileShader;
            result->pGLCreateShader     = entry_points->pGLCreateShader;
            result->pGLDeleteShader     = entry_points->pGLDeleteShader;
            result->pGLGetError         = entry_points->pGLGetError;
            result->pGLGetShaderInfoLog = entry_points->pGLGetShaderInfoLog;
            result->pGLGetShaderiv      = entry_points->pGLGetShaderiv;
            result->pGLShaderSource     = entry_points->pGLShaderSource;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            result->pGLCompileShader    = entry_points->pGLCompileShader;
            result->pGLCreateShader     = entry_points->pGLCreateShader;
            result->pGLDeleteShader     = entry_points->pGLDeleteShader;
            result->pGLGetError         = entry_points->pGLGetError;
            result->pGLGetShaderInfoLog = entry_points->pGLGetShaderInfoLog;
            result->pGLGetShaderiv      = entry_points->pGLGetShaderiv;
            result->pGLShaderSource     = entry_points->pGLShaderSource;
        }

        /* Carry on */
        ogl_context_request_callback_from_context_thread(context,
                                                         _ogl_shader_create_callback,
                                                         result);

        /* Register the shader object */
        ogl_shaders_register_shader(shaders,
                                    (ogl_shader) result);
    }

    return (ogl_shader) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_shader_get_body(__in __notnull ogl_shader shader)
{
    return ((_ogl_shader*)shader)->body;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_get_compile_status(__in __notnull ogl_shader shader)
{
    return ((_ogl_shader*) shader)->compile_status;
}

/** Please see header for specification */
PUBLIC GLuint ogl_shader_get_id(__in __notnull ogl_shader shader)
{
    return ((_ogl_shader*)shader)->id;
}

/** Please see header for specification */
PUBLIC EMERALD_API const char* ogl_shader_get_shader_info_log(__in __notnull ogl_shader shader)
{
    const char*  result     = NULL;
    _ogl_shader* shader_ptr = (_ogl_shader*) shader;

    ASSERT_DEBUG_SYNC(shader_ptr->has_been_compiled,
                      "Cannot retrieve shader info log - shader was not compiled!");

    if (shader_ptr->has_been_compiled)
    {
        result = shader_ptr->shader_info_log;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_shader_get_name(__in __notnull ogl_shader shader)
{
    return ((_ogl_shader*)shader)->name;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader_type ogl_shader_get_type(__in __notnull ogl_shader shader)
{
    return ((_ogl_shader*)shader)->type;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_set_body(__in __notnull ogl_shader                shader,
                                            __in __notnull system_hashed_ansi_string body)
{
    bool         result     = false;
    _ogl_shader* shader_ptr = (_ogl_shader*) shader;
    
    if (body != NULL && system_hashed_ansi_string_get_length(body) > 0)
    {
        if (shader_ptr->id != 0)
        {
            shader_ptr->body = body;
            result           = true;

            ogl_context_request_callback_from_context_thread(shader_ptr->context,
                                                             _ogl_shader_set_body_callback,
                                                             shader_ptr);
        }
        else
        {
            LOG_ERROR("Cannot assign body to a shader with incorrect id.");
        }
    }
    else
    {
        LOG_ERROR("Empty body cannot be assigned to a shader!");
    }

    return true;
}