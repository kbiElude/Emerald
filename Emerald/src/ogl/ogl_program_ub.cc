/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_program_block.h"
#include "ogl/ogl_program_ub.h"


/** TODO */
PRIVATE ogl_program_block_property _ogl_program_ub_get_block_property_for_ub_property(__in ogl_program_ub_property property)
{
    ogl_program_block_property result;

    switch (property)
    {
        case OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_BLOCK_DATA_SIZE;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_BO_ID:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_BO_ID;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_BO_START_OFFSET;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_INDEX:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_INDEX;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_INDEXED_UB_BP:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_INDEXED_UB_BP;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_NAME:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_NAME;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_N_MEMBERS:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_N_MEMBERS;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_program_ub_property value.");
        }
    } /* switch (property) */

    return result;
}


/** Please see header for spec */
PUBLIC ogl_program_ub ogl_program_ub_create(__in __notnull ogl_context               context,
                                            __in __notnull ogl_program               owner_program,
                                            __in __notnull unsigned int              ub_index,
                                            __in __notnull system_hashed_ansi_string ub_name,
                                            __in           bool                      support_sync_behavior)
{
    return (ogl_program_ub) ogl_program_block_create(context,
                                                      owner_program,
                                                      OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                                      ub_index,
                                                      ub_name,
                                                      support_sync_behavior);
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_get_property(__in  __notnull const ogl_program_ub    ub,
                                                    __in            ogl_program_ub_property property,
                                                    __out __notnull void*                   out_result)
{
    ogl_program_block_get_property((ogl_program_block) ub,
                                   _ogl_program_ub_get_block_property_for_ub_property(property),
                                   out_result);
}

/** Please see header for spec */
PUBLIC void ogl_program_ub_release(__in __notnull ogl_program_ub ub)
{
    ogl_program_block_release( (ogl_program_block) ub);
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_set_arrayed_uniform_value(__in                       __notnull ogl_program_ub ub,
                                                                 __in                                 GLuint         ub_uniform_offset,
                                                                 __in_ecount(src_data_size) __notnull const void*    src_data,
                                                                 __in                                 int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                 __in                                 unsigned int   src_data_size,
                                                                 __in                                 unsigned int   dst_array_start_index,
                                                                 __in                                 unsigned int   dst_array_item_count)
{
    ASSERT_DEBUG_SYNC(src_data_flags == 0,
                      "Unsupported src_data_flags argument value");

    ogl_program_block_set_arrayed_variable_value( (ogl_program_block) ub,
                                                  ub_uniform_offset,
                                                  src_data,
                                                  src_data_size,
                                                  dst_array_start_index,
                                                  dst_array_item_count);
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_set_nonarrayed_uniform_value(__in                       __notnull ogl_program_ub ub,
                                                                    __in                                 GLuint         ub_uniform_offset,
                                                                    __in_ecount(src_data_size) __notnull const void*    src_data,
                                                                    __in                                 int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                    __in                                 unsigned int   src_data_size)
{
    ASSERT_DEBUG_SYNC(src_data_flags == 0,
                      "Unsupported src_data_flags argument value");

    ogl_program_block_set_nonarrayed_variable_value( (ogl_program_block) ub,
                                                     ub_uniform_offset,
                                                     src_data,
                                                     src_data_size);
}

/* Please see header for spec */
PUBLIC void ogl_program_ub_set_property(__in  __notnull const ogl_program_ub    ub,
                                        __in            ogl_program_ub_property property,
                                        __out __notnull const void*             data)
{
    ogl_program_block_set_property( (ogl_program_block) ub,
                                    _ogl_program_ub_get_block_property_for_ub_property(property),
                                    data);
}

/* Please see header for spec */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void ogl_program_ub_sync(__in __notnull ogl_program_ub ub)
{
    ogl_program_block_sync( (ogl_program_block) ub);
}
