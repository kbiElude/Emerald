/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_program_block.h"
#include "ogl/ogl_program_ssb.h"


/** TODO */
PRIVATE ogl_program_block_property _ogl_program_ssb_get_block_property_for_ssb_property(ogl_program_ssb_property property)
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
PUBLIC ogl_program_ssb ogl_program_ssb_create(ral_context               context,
                                              raGL_program              owner_program,
                                              unsigned int              ssb_index,
                                              system_hashed_ansi_string ssb_name)
{
    return (ogl_program_ssb) ogl_program_block_create(context,
                                                      owner_program,
                                                      OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER,
                                                      ssb_index,
                                                      ssb_name,
                                                      false); /* supports_sync_behavior */
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ssb_get_property(const ogl_program_ssb    ssb,
                                                     ogl_program_ssb_property property,
                                                     void*                    out_result)
{
    ogl_program_block_get_property((ogl_program_block) ssb,
                                   _ogl_program_ssb_get_block_property_for_ssb_property(property),
                                   out_result);
}

/** Please see header for spec */
PUBLIC EMERALD_API bool ogl_program_ssb_get_variable_by_index(const ogl_program_ssb        ssb,
                                                              unsigned int                 n_variable,
                                                              const ral_program_variable** out_variable_ptr)
{
    return ogl_program_block_get_block_variable((ogl_program_block) ssb,
                                                n_variable,
                                                out_variable_ptr);
}

/** Please see header for spec */
PUBLIC void ogl_program_ssb_release(ogl_program_ssb ssb)
{
    ogl_program_block_release( (ogl_program_block) ssb);
}

/* Please see header for spec */
PUBLIC void ogl_program_ssb_set_property(const ogl_program_ssb    ssb,
                                         ogl_program_ssb_property property,
                                         const void*              data)
{
    ogl_program_block_set_property( (ogl_program_block) ssb,
                                    _ogl_program_ssb_get_block_property_for_ssb_property(property),
                                    data);
}
