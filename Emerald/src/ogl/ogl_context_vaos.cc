/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_vaos.h"
#include "ogl/ogl_vao.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include <sstream>

/* Private type definitions */

/** TODO */
typedef struct _ogl_context_vaos
{
    ogl_context      context; /* Do NOT retain */
    system_hash64map vaos;

    explicit _ogl_context_vaos(__in __notnull ogl_context in_context)
    {
        context  = in_context;
        vaos     = system_hash64map_create(sizeof(ogl_vao) );
    }

    ~_ogl_context_vaos()
    {
        LOG_INFO("VAO manager deallocating..");

        if (vaos != NULL)
        {
            /* Release any outstanding VAO descriptors. The corresponding hash-map entry will be removed
             * via a call-back coming from the released ogl_vao instance, so just keep deleting items until
             * the map clears out.
             */
            while (system_hash64map_get_amount_of_elements(vaos) > 0)
            {
                ogl_vao vao_instance = NULL;

                if (!system_hash64map_get_element_at(vaos,
                                                     0,            /* n */
                                                    &vao_instance,
                                                     NULL) )       /* pOutHash */
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "VAO map clean-up failure.");

                    break;
                }

                ogl_vao_release(vao_instance);
            } /* while (VAO descriptors remain) */

            system_hash64map_release(vaos);

            vaos = NULL;
        } /* if (vaos != NULL) */
    }
} _ogl_context_vaos;


/** Please see header for specification */
PUBLIC void ogl_context_vaos_add_vao(__in __notnull ogl_context_vaos vaos,
                                     __in           GLuint           gl_id,
                                     __in __notnull ogl_vao          vao)
{
    _ogl_context_vaos* vaos_ptr = (_ogl_context_vaos*) vaos;

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(vaos_ptr->vaos,
                                                 gl_id),
                      "VAO with ID [%d] is already recognized.",
                      gl_id);

    system_hash64map_insert(vaos_ptr->vaos,
                            (system_hash64) gl_id,
                            vao,
                            NULL,  /* on_remove_callback */
                            NULL); /* on_remove_callback_user_arg */

    LOG_INFO("New VAO (id:[%d]) was registered in VAO cache.",
             gl_id);
}

/** Please see header for specification */
PUBLIC ogl_context_vaos ogl_context_vaos_create(__in __notnull ogl_context context)
{
    _ogl_context_vaos* vaos_ptr = new (std::nothrow) _ogl_context_vaos(context);

    ASSERT_DEBUG_SYNC(vaos_ptr != NULL,
                      "Out of memory");

    if (vaos_ptr != NULL)
    {
        /* Initialize the default zero-ID VAO */
        ogl_vao zero_vao = ogl_vao_create(context,
                                          0); /* gl_id */

        ogl_context_vaos_add_vao((ogl_context_vaos) vaos_ptr,
                                 0,         /* gl_id */
                                 zero_vao);
    }

    return (ogl_context_vaos) vaos_ptr;
}

/** Please see header for specification */
PUBLIC void ogl_context_vaos_delete_vao(__in __notnull ogl_context_vaos vaos,
                                        __in           GLuint           gl_id)
{
    _ogl_context_vaos* vaos_ptr = (_ogl_context_vaos*) vaos;

    if (!system_hash64map_remove(vaos_ptr->vaos,
                                 gl_id) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "The requested VAO at index [%d] was not found",
                          gl_id);
    }
    else
    {
        LOG_INFO("VAO (id:[%d]) was removed from the VAO cache.",
                  gl_id);
    }
}

/** Please see header for specification */
PUBLIC ogl_vao ogl_context_vaos_get_vao(__in __notnull ogl_context_vaos vaos,
                                        __in           GLuint           gl_id)
{
    ogl_vao            result   = NULL;
    _ogl_context_vaos* vaos_ptr = (_ogl_context_vaos*) vaos;

    system_hash64map_get(vaos_ptr->vaos,
                         (system_hash64) gl_id,
                        &result);

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_context_vaos_release(__in __notnull ogl_context_vaos vaos)
{
    if (vaos != NULL)
    {
        delete (_ogl_context_vaos*) vaos;

        vaos = NULL;
    }
}
