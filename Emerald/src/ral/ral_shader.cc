/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ral/ral_shader.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_read_write_mutex.h"


typedef struct _ral_shader
{
    system_callback_manager   callback_manager;
    system_hashed_ansi_string name;
    ral_shader_source         source;
    ral_shader_type           type;

    system_critical_section   cs;
    system_hashed_ansi_string glsl_body;
    system_read_write_mutex   glsl_body_lock;

    explicit _ral_shader(const ral_shader_create_info* shader_create_info_ptr)
    {
        callback_manager = system_callback_manager_create((_callback_id) RAL_SHADER_CALLBACK_ID_COUNT);
        cs               = system_critical_section_create();
        glsl_body        = NULL;
        glsl_body_lock   = system_read_write_mutex_create();
        name             = shader_create_info_ptr->name;
        source           = shader_create_info_ptr->source;
        type             = shader_create_info_ptr->type;
    }

    ~_ral_shader()
    {
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        }

        if (cs != NULL)
        {
            system_critical_section_release(cs);

            cs = NULL;
        }

        if (glsl_body_lock != NULL)
        {
            system_read_write_mutex_release(glsl_body_lock);

            glsl_body_lock = NULL;
        }
    }
} _ral_shader;


/** Please see header for specification */
PRIVATE void _ral_shader_release(void* shader)
{
    delete (_ral_shader*) shader;
}


/** Please see header for specification */
PUBLIC ral_shader ral_shader_create(const ral_shader_create_info* shader_create_info_ptr)
{
    _ral_shader* shader_ptr = new (std::nothrow) _ral_shader(shader_create_info_ptr);

    ASSERT_ALWAYS_SYNC(shader_ptr != NULL,
                       "Out of memory");

    return (ral_shader) shader_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_shader_get_property(ral_shader          shader,
                                                ral_shader_property property,
                                                void*               out_result_ptr)
{
    const _ral_shader* shader_ptr = (_ral_shader*) shader;

    /* Sanity checks */
    if (shader == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input shader instance is NULL");

        goto end;
    }

    /* Retrieve the requested value */
    switch (property)
    {
        case RAL_SHADER_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = shader_ptr->callback_manager;

            break;
        }

        case RAL_SHADER_PROPERTY_GLSL_BODY:
        {
            ASSERT_DEBUG_SYNC(shader_ptr->source == RAL_SHADER_SOURCE_GLSL,
                              "RAL_SHADER_PROPERTY_GLSL_BODY queries are only valid for GLSL-backed ral_shader instances");

            *(system_hashed_ansi_string*) out_result_ptr = shader_ptr->glsl_body;

            break;
        }

        case RAL_SHADER_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = shader_ptr->name;

            break;
        }

        case RAL_SHADER_PROPERTY_SOURCE:
        {
            *(ral_shader_source*) out_result_ptr = shader_ptr->source;

            break;
        }

        case RAL_SHADER_PROPERTY_TYPE:
        {
            *(ral_shader_type*) out_result_ptr = shader_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_shader_property value.");
        }
    } /* switch (property) */
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_shader_lock(ral_shader shader)
{
    system_read_write_mutex_lock( ((_ral_shader*) shader)->glsl_body_lock,
                                 ACCESS_WRITE);
}

/** Please see header for specification */
PUBLIC void ral_shader_release(ral_shader& shader)
{
    delete (_ral_shader*) shader;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_shader_set_property(ral_shader          shader,
                                                ral_shader_property property,
                                                const void*         data)
{
    _ral_shader* shader_ptr = (_ral_shader*) shader;

    if (shader == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_shader instance is NULL");

        goto end;
    }

    system_critical_section_enter(shader_ptr->cs);

    switch (property)
    {
        case RAL_SHADER_PROPERTY_GLSL_BODY:
        {
            system_hashed_ansi_string in_body;

            if (         data == NULL ||
                *(void**)data == NULL)
            {
                in_body = NULL;
            }
            else
            {
                in_body = *(const system_hashed_ansi_string*) data;
            }

            ASSERT_DEBUG_SYNC(shader_ptr->source == RAL_SHADER_SOURCE_GLSL,
                              "RAL_SHADER_PROPERTY_GLSL_BODY queries are only valid for GLSL-backed ral_shader instances");

            if (shader_ptr->glsl_body != in_body)
            {
                system_read_write_mutex_lock( ((_ral_shader*) shader)->glsl_body_lock,
                                             ACCESS_WRITE);
                {
                    shader_ptr->glsl_body = in_body;
                }
                system_read_write_mutex_unlock( ((_ral_shader*) shader)->glsl_body_lock,
                                               ACCESS_WRITE);

                if (in_body != NULL)
                {
                    system_callback_manager_call_back(shader_ptr->callback_manager,
                                                      RAL_SHADER_CALLBACK_ID_GLSL_BODY_UPDATED,
                                                      shader_ptr);
                }
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognzied ral_shader_property value.");
        }
    } /* switch (property) */

    system_critical_section_leave(shader_ptr->cs);
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_shader_unlock(ral_shader shader)
{
    system_read_write_mutex_unlock( ((_ral_shader*) shader)->glsl_body_lock,
                                   ACCESS_WRITE);
}
