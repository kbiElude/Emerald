/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_shader.h"
#include "raGL/raGL_sync.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_shader.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"


/** Internal type definitions */
typedef struct
{
    system_callback_manager   callback_manager;
    bool                      compile_status;
    ogl_context               context; /* DO NOT retain - context owns the instance */
    bool                      has_been_compiled;
    GLuint                    id;
    system_hashed_ansi_string last_compiled_body;
    char*                     shader_info_log;
    ral_shader                shader_ral; /* DO NOT retain/release */

    /* GL entry-point cache */
    PFNGLCOMPILESHADERPROC    pGLCompileShader;
    PFNGLCREATESHADERPROC     pGLCreateShader;
    PFNGLDELETESHADERPROC     pGLDeleteShader;
    PFNGLGETSHADERINFOLOGPROC pGLGetShaderInfoLog;
    PFNGLGETSHADERIVPROC      pGLGetShaderiv;
    PFNGLSHADERSOURCEPROC     pGLShaderSource;

} _raGL_shader;


/** TODO */
PRIVATE ral_present_job _raGL_shader_compile_callback(ral_context                                                context,
                                                      void*                                                      in_arg,
                                                      const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_shader* shader_ptr          = reinterpret_cast<_raGL_shader*>(in_arg);
    const char*   shader_body_raw_ptr = system_hashed_ansi_string_get_buffer(shader_ptr->last_compiled_body);

    raGL_backend_sync();

    shader_ptr->pGLShaderSource (shader_ptr->id,
                                 1, /* count */
                                &shader_body_raw_ptr,
                                 nullptr);
    shader_ptr->pGLCompileShader(shader_ptr->id);

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

    if (shader_ptr->shader_info_log != nullptr)
    {
        delete shader_ptr->shader_info_log;
    }

    if (shader_info_log_length > 1)
    {
        shader_ptr->shader_info_log = new (std::nothrow) char[shader_info_log_length + 1];

        ASSERT_ALWAYS_SYNC(shader_ptr->shader_info_log != nullptr,
                           "Out of memory while allocating space for shader info log [%d bytes]",
                           shader_info_log_length+1);

        if (shader_ptr->shader_info_log != nullptr)
        {
            memset(shader_ptr->shader_info_log,
                   0,
                   shader_info_log_length + 1);

            shader_ptr->pGLGetShaderInfoLog(shader_ptr->id,
                                            shader_info_log_length + 1,
                                            nullptr,
                                            shader_ptr->shader_info_log);

            LOG_ERROR("Shader info log for shader [%u] follows: \n>>\n%s\n<<",
                      shader_ptr->id,
                      shader_ptr->shader_info_log);
        }
    }

    /* Notify contexts about the update */
    raGL_backend_enqueue_sync();

    /* We fire GL calls directly in this func, so no need to return a present job */
    return nullptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_shader_create_callback(ral_context                                                context,
                                                     void*                                                      in_arg,
                                                     const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_shader*     shader_ptr    = reinterpret_cast<_raGL_shader*>(in_arg);
    ral_shader_source shader_source = RAL_SHADER_SOURCE_UNKNOWN;
    ral_shader_type   shader_type   = RAL_SHADER_TYPE_UNKNOWN;

    ral_shader_get_property(shader_ptr->shader_ral,
                            RAL_SHADER_PROPERTY_SOURCE,
                           &shader_source);
    ral_shader_get_property(shader_ptr->shader_ral,
                            RAL_SHADER_PROPERTY_TYPE,
                           &shader_type);

    ASSERT_DEBUG_SYNC(shader_source == RAL_SHADER_SOURCE_GLSL,
                      "Only GLSL-sourced shaders are supported by OpenGL backend.");

    /* Acquire a new ID */
    shader_ptr->id = shader_ptr->pGLCreateShader(raGL_utils_get_ogl_enum_for_ral_shader_type(shader_type));

    /* Force other contexts to sync */
    raGL_backend_enqueue_sync();

    /* We fire GL calls directly in this func, so no need to return a present job */
    return nullptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_shader_release_callback(ral_context                                                context,
                                                      void*                                                      in_arg,
                                                      const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    _raGL_shader* shader_ptr = reinterpret_cast<_raGL_shader*>(in_arg);

    /* Purge the shader object */
    shader_ptr->pGLDeleteShader(shader_ptr->id);

    /* Force other contexts to sync */
    raGL_backend_enqueue_sync();
}

/** TODO */
PRIVATE void _raGL_shader_release(void* shader)
{
    ral_context           context_ral       = nullptr;
    ral_rendering_handler rendering_handler = nullptr;
    _raGL_shader*         shader_ptr        = reinterpret_cast<_raGL_shader*>(shader);

    if (shader_ptr->callback_manager != nullptr)
    {
        system_callback_manager_release(shader_ptr->callback_manager);

        shader_ptr->callback_manager = nullptr;
    }

    if (shader_ptr->shader_info_log != nullptr)
    {
        delete shader_ptr->shader_info_log;

        shader_ptr->shader_info_log = nullptr;
    }

    ogl_context_get_property(shader_ptr->context,
                             OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                            &context_ral);
    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                     _raGL_shader_release_callback,
                                                     shader_ptr,
                                                     false); /* present_after_executed */
}

/** Please see header for specification */
PUBLIC bool raGL_shader_compile(raGL_shader shader)
{
    ogl_context               current_context = ogl_context_get_current_context();
    bool                      result          = false;
    system_hashed_ansi_string shader_body     = nullptr;
    _raGL_shader*             shader_ptr      = reinterpret_cast<_raGL_shader*>(shader);

    ral_shader_get_property(shader_ptr->shader_ral,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &shader_body);

    if (!shader_ptr->has_been_compiled                                                  ||
         shader_ptr->has_been_compiled && shader_ptr->last_compiled_body != shader_body)
    {
        shader_ptr->has_been_compiled  = false;
        shader_ptr->last_compiled_body = shader_body;
    }
    else
    {
        /* Specified raGL shader has already been compiled. */
        result = true;

        goto end;
    }

    if (!shader_ptr->has_been_compiled)
    {
        ral_context           context_ral       = nullptr;
        ral_rendering_handler rendering_handler = nullptr;

        if (shader_ptr->context != current_context)
        {
            current_context = shader_ptr->context;
        }

        ogl_context_get_property(current_context,
                                 OGL_CONTEXT_PROPERTY_CONTEXT_RAL,
                                &context_ral);
        ral_context_get_property(context_ral,
                                 RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                                &rendering_handler);

        ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                         _raGL_shader_compile_callback,
                                                         shader_ptr,
                                                         false); /* present_after_executed */

        result                        = shader_ptr->compile_status;
        shader_ptr->has_been_compiled = true;

        system_callback_manager_call_back(shader_ptr->callback_manager,
                                          RAGL_SHADER_CALLBACK_ID_SHADER_COMPILED,
                                          shader);
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC raGL_shader raGL_shader_create(ral_context context_ral,
                                      ogl_context context_ogl,
                                      ral_shader  shader_ral)
{
    ral_rendering_handler     rendering_handler = nullptr;
    system_hashed_ansi_string shader_name       = nullptr;

    ral_context_get_property(context_ral,
                             RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);
    ral_shader_get_property (shader_ral,
                             RAL_SHADER_PROPERTY_NAME,
                            &shader_name);

    ASSERT_DEBUG_SYNC(ral_context_get_shader_by_name(context_ral,
                                                     shader_name) == nullptr,
                      "Shader [%s] has already been created!",
                      system_hashed_ansi_string_get_buffer(shader_name) );

    /* Carry on and create a new shader instance */
    _raGL_shader* new_shader_ptr = new (std::nothrow) _raGL_shader;

    ASSERT_ALWAYS_SYNC(new_shader_ptr != nullptr,
                       "Out of memory");

    if (new_shader_ptr != nullptr)
    {
        new_shader_ptr->callback_manager   = system_callback_manager_create((_callback_id) RAGL_SHADER_CALLBACK_ID_COUNT);
        new_shader_ptr->compile_status     = false;
        new_shader_ptr->context            = context_ogl;
        new_shader_ptr->has_been_compiled  = false;
        new_shader_ptr->id                 = 0;
        new_shader_ptr->last_compiled_body = nullptr;
        new_shader_ptr->shader_ral         = shader_ral;
        new_shader_ptr->shader_info_log    = nullptr;

        /* Initialize entry-point cache */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ogl_context_get_property(context_ogl,
                                 OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points_ptr = nullptr;

            ogl_context_get_property(context_ogl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points_ptr);

            new_shader_ptr->pGLCompileShader    = entry_points_ptr->pGLCompileShader;
            new_shader_ptr->pGLCreateShader     = entry_points_ptr->pGLCreateShader;
            new_shader_ptr->pGLDeleteShader     = entry_points_ptr->pGLDeleteShader;
            new_shader_ptr->pGLGetShaderInfoLog = entry_points_ptr->pGLGetShaderInfoLog;
            new_shader_ptr->pGLGetShaderiv      = entry_points_ptr->pGLGetShaderiv;
            new_shader_ptr->pGLShaderSource     = entry_points_ptr->pGLShaderSource;
        }
        else
        {
            const ogl_context_gl_entrypoints* entry_points_ptr = nullptr;

            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized backendtype");

            ogl_context_get_property(context_ogl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points_ptr);

            new_shader_ptr->pGLCompileShader    = entry_points_ptr->pGLCompileShader;
            new_shader_ptr->pGLCreateShader     = entry_points_ptr->pGLCreateShader;
            new_shader_ptr->pGLDeleteShader     = entry_points_ptr->pGLDeleteShader;
            new_shader_ptr->pGLGetShaderInfoLog = entry_points_ptr->pGLGetShaderInfoLog;
            new_shader_ptr->pGLGetShaderiv      = entry_points_ptr->pGLGetShaderiv;
            new_shader_ptr->pGLShaderSource     = entry_points_ptr->pGLShaderSource;
        }

        /* Carry on */
        ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                         _raGL_shader_create_callback,
                                                         new_shader_ptr,
                                                         false); /* present_after_executed */
    }

    return (raGL_shader) new_shader_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_shader_get_property(const raGL_shader    shader,
                                     raGL_shader_property property,
                                     void*                out_result_ptr)
{
    const _raGL_shader* shader_ptr = reinterpret_cast<const _raGL_shader*>(shader);

    if (shader_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(shader_ptr != nullptr,
                          "Input raGL_shader instance is NULL");

        goto end;
    }

    switch (property)
    {
        case RAGL_SHADER_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = shader_ptr->callback_manager;

            break;
        }

        case RAGL_SHADER_PROPERTY_COMPILE_STATUS:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = shader_ptr->compile_status;

            break;
        }

        case RAGL_SHADER_PROPERTY_ID:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = shader_ptr->id;

            break;
        }

        case RAGL_SHADER_PROPERTY_INFO_LOG:
        {
            ASSERT_DEBUG_SYNC(shader_ptr->has_been_compiled,
                              "Cannot retrieve shader info log - shader has not been compiled yet!");

            *reinterpret_cast<const char**>(out_result_ptr) = shader_ptr->shader_info_log;

            break;
        }

        case RAGL_SHADER_PROPERTY_SHADER_RAL:
        {
            *reinterpret_cast<ral_shader*>(out_result_ptr) = shader_ptr->shader_ral;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_shader_property value.");
        }
    }

end:
    ;
}

/* Please see header for specification */
PUBLIC void raGL_shader_release(raGL_shader shader)
{
    delete reinterpret_cast<_raGL_shader*>(shader);
}