/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_program_block.h"
#include "ogl/ogl_program_ssb.h"


/** TODO */
PRIVATE ogl_program_block_property _ogl_program_ssb_get_block_property_for_ssb_property(__in ogl_program_ssb_property property)
{
    ogl_program_block_property result;

    switch (property)
    {
        case OGL_PROGRAM_SSB_PROPERTY_BLOCK_DATA_SIZE:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_BLOCK_DATA_SIZE;

            break;
        }

        case OGL_PROGRAM_SSB_PROPERTY_INDEX:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_INDEX;

            break;
        }

        case OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP;

            break;
        }

        case OGL_PROGRAM_SSB_PROPERTY_NAME:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_NAME;

            break;
        }

        case OGL_PROGRAM_SSB_PROPERTY_N_MEMBERS:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_N_MEMBERS;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_program_ssb_property value.");
        }
    } /* switch (property) */

    return result;
}


/** Please see header for spec */
PUBLIC ogl_program_ssb ogl_program_ssb_create(__in __notnull ogl_context               context,
                                              __in __notnull ogl_program               owner_program,
                                              __in __notnull unsigned int              ssb_index,
                                              __in __notnull system_hashed_ansi_string ssb_name)
{
    return (ogl_program_ssb) ogl_program_block_create(context,
                                                      owner_program,
                                                      OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER,
                                                      ssb_index,
                                                      ssb_name,
                                                      false); /* supports_sync_behavior */
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ssb_get_property(__in  __notnull const ogl_program_ssb    ssb,
                                                     __in            ogl_program_ssb_property property,
                                                     __out __notnull void*                    out_result)
{
    ogl_program_block_get_property((ogl_program_block) ssb,
                                   _ogl_program_ssb_get_block_property_for_ssb_property(property),
                                   out_result);
}

/** Please see header for spec */
PUBLIC void ogl_program_ssb_release(__in __notnull ogl_program_ssb ssb)
{
    ogl_program_block_release( (ogl_program_block) ssb);
}

/* Please see header for spec */
PUBLIC void ogl_program_ssb_set_property(__in  __notnull const ogl_program_ssb    ssb,
                                         __in            ogl_program_ssb_property property,
                                         __out __notnull const void*              data)
{
    ogl_program_block_set_property( (ogl_program_block) ssb,
                                    _ogl_program_ssb_get_block_property_for_ssb_property(property),
                                    data);
}
