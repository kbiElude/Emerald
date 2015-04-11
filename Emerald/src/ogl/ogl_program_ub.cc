/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_memory_manager.h"
#include "system/system_resizable_vector.h"

/** Internal types */
typedef struct _ogl_program_ub
{
    ogl_context  context; /* NOT retained */
    unsigned int index;
    ogl_program  owner;

    GLint                     block_data_size;
    system_hashed_ansi_string name;
    system_resizable_vector   members; /* pointers to const ogl_program_uniform_descriptor*. These are owned by owner - DO NOT release. */

    PFNGLGETACTIVEUNIFORMBLOCKIVPROC pGLGetActiveUniformBlockiv;

    explicit _ogl_program_ub(__in __notnull ogl_context               in_context,
                             __in __notnull ogl_program               in_owner,
                             __in __notnull unsigned int              in_index,
                             __in __notnull system_hashed_ansi_string in_name)
    {
        block_data_size = 0;
        context         = in_context;
        index           = in_index;
        name            = in_name;
        members         = system_resizable_vector_create(4, /* capacity */
                                                         sizeof(ogl_program_uniform_descriptor*) );
        owner           = in_owner;

        pGLGetActiveUniformBlockiv = NULL;

        ASSERT_DEBUG_SYNC(members != NULL,
                          "Out of memory");
    }

    ~_ogl_program_ub()
    {
        if (members != NULL)
        {
            /* No need to release items - these are owned by ogl_program! */
            system_resizable_vector_release(members);

            members = NULL;
        }
    }
} _ogl_program_ub;


/** TODO */
PRIVATE bool _ogl_program_ub_init(__in __notnull _ogl_program_ub* ub_ptr)
{
    bool result = true;

    ASSERT_DEBUG_SYNC(ub_ptr != NULL,
                      "Input argument is NULL");

    /* Retrieve entry-points */
    ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

    ogl_context_get_property(ub_ptr->context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_GL)
    {
        const ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(ub_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        ub_ptr->pGLGetActiveUniformBlockiv = entry_points->pGLGetActiveUniformBlockiv;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_ES,
                          "Unrecognized rendering context type");

        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(ub_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        ub_ptr->pGLGetActiveUniformBlockiv = entry_points->pGLGetActiveUniformBlockiv;
    }

    /* Retrieve UB uniforrms */
    GLint  n_active_uniform_blocks           = 0;
    GLint  n_active_uniform_block_max_length = 0;
    GLuint po_id                             = ogl_program_get_id(ub_ptr->owner);

    ub_ptr->pGLGetActiveUniformBlockiv(po_id,
                                       ub_ptr->index,
                                       GL_UNIFORM_BLOCK_DATA_SIZE,
                                      &ub_ptr->block_data_size);

    /* Determine all uniform members of the block */
    GLint* active_uniform_indices = NULL;
    GLint  n_active_ub_uniforms   = 0;

    ub_ptr->pGLGetActiveUniformBlockiv(po_id,
                                       ub_ptr->index,
                                       GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
                                      &n_active_ub_uniforms);

    if (n_active_ub_uniforms > 0)
    {
        active_uniform_indices = new (std::nothrow) GLint[n_active_ub_uniforms];

        if (active_uniform_indices == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            result = false;
            goto end;
        } /* if (active_uniform_indices == NULL) */

        if (active_uniform_indices != NULL)
        {
            ub_ptr->pGLGetActiveUniformBlockiv(po_id,
                                               ub_ptr->index,
                                               GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                                               active_uniform_indices);

            for (int n_active_uniform = 0;
                     n_active_uniform < n_active_ub_uniforms;
                   ++n_active_uniform)
            {
                const ogl_program_uniform_descriptor* uniform_ptr = NULL;

                if (!ogl_program_get_uniform_by_index(ub_ptr->owner,
                                                      active_uniform_indices[n_active_uniform],
                                                     &uniform_ptr) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve uniform descriptor for index [%d]",
                                      active_uniform_indices[n_active_uniform]);

                    result = false;
                    goto end;
                }
                else
                {
                    system_resizable_vector_push(ub_ptr->members,
                                                 (void*) uniform_ptr);
                }
            } /* for (all uniform block members) */

            delete [] active_uniform_indices;
            active_uniform_indices = NULL;
        }
    } /* if (n_active_ub_uniforms > 0) */

    /* All done */
end:
    return result;
}


/** Please see header for spec */
PUBLIC ogl_program_ub ogl_program_ub_create(__in __notnull ogl_context               context,
                                            __in __notnull ogl_program               owner_program,
                                            __in __notnull unsigned int              ub_index,
                                            __in __notnull system_hashed_ansi_string ub_name)
{
    _ogl_program_ub* new_ub_ptr = new (std::nothrow) _ogl_program_ub(context,
                                                                     owner_program,
                                                                     ub_index,
                                                                     ub_name);

    ASSERT_DEBUG_SYNC(new_ub_ptr != NULL,
                      "Out of memory");

    if (new_ub_ptr != NULL)
    {
        _ogl_program_ub_init(new_ub_ptr);
    } /* if (new_ub_ptr != NULL) */

    return (ogl_program_ub) new_ub_ptr;
}

/** Please see header for spec */
PUBLIC void ogl_program_ub_get_property(__in  __notnull const ogl_program_ub    ub,
                                        __in            ogl_program_ub_property property,
                                        __out __notnull void*                   out_result)
{
    const _ogl_program_ub* ub_ptr = (const _ogl_program_ub*) ub;

    switch (property)
    {
        case OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE:
        {
            *(unsigned int*) out_result = ub_ptr->block_data_size;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = ub_ptr->name;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_N_MEMBERS:
        {
            *(unsigned int*) out_result = system_resizable_vector_get_amount_of_elements(ub_ptr->members);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_program_ub_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void ogl_program_ub_release(__in __notnull ogl_program_ub ub)
{
    delete (_ogl_program_ub*) ub;
}
