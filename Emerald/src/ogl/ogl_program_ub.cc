/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_program_block.h"
#include "ogl/ogl_program_ub.h"


/** TODO */
PRIVATE ogl_program_block_property _ogl_program_ub_get_block_property_for_ub_property(ogl_program_ub_property property)
{
    ogl_program_block_property result;

    switch (property)
    {
        case OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_BLOCK_DATA_SIZE;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_BUFFER_RAL;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_INDEX:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_INDEX;

            break;
        }

        case OGL_PROGRAM_UB_PROPERTY_INDEXED_BP:
        {
            result = OGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP;

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
PUBLIC ogl_program_ub ogl_program_ub_create(ral_context               context,
                                            raGL_program              owner_program,
                                            unsigned int              ub_index,
                                            system_hashed_ansi_string ub_name,
                                            bool                      support_sync_behavior)
{
    return (ogl_program_ub) ogl_program_block_create(context,
                                                     owner_program,
                                                     OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER,
                                                     ub_index,
                                                     ub_name,
                                                     support_sync_behavior);
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_get_property(const ogl_program_ub    ub,
                                                    ogl_program_ub_property property,
                                                    void*                   out_result)
{
    ogl_program_block_get_property((ogl_program_block) ub,
                                   _ogl_program_ub_get_block_property_for_ub_property(property),
                                   out_result);
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_get_variable_by_name(ogl_program_ub               ub,
                                                            system_hashed_ansi_string    name,
                                                            const ral_program_variable** out_variable_ptr)
{
    ogl_program_block_get_block_variable_by_name((ogl_program_block) ub,
                                                 name,
                                                 out_variable_ptr);
}

/** Please see header for spec */
PUBLIC void ogl_program_ub_release(ogl_program_ub ub)
{
    ogl_program_block_release( (ogl_program_block) ub);
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_program_ub_set_arrayed_uniform_value(ogl_program_ub ub,
                                                                 GLuint         ub_uniform_offset,
                                                                 const void*    src_data,
                                                                 int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                 unsigned int   src_data_size,
                                                                 unsigned int   dst_array_start_index,
                                                                 unsigned int   dst_array_item_count)
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
PUBLIC EMERALD_API void ogl_program_ub_set_nonarrayed_uniform_value(ogl_program_ub ub,
                                                                    GLuint         ub_uniform_offset,
                                                                    const void*    src_data,
                                                                    int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                    unsigned int   src_data_size)
{
    ASSERT_DEBUG_SYNC(src_data_flags == 0,
                      "Unsupported src_data_flags argument value");

    ogl_program_block_set_nonarrayed_variable_value( (ogl_program_block) ub,
                                                     ub_uniform_offset,
                                                     src_data,
                                                     src_data_size);
}

/* Please see header for spec */
PUBLIC void ogl_program_ub_set_property(const ogl_program_ub    ub,
                                        ogl_program_ub_property property,
                                        const void*             data)
{
    ogl_program_block_set_property( (ogl_program_block) ub,
                                    _ogl_program_ub_get_block_property_for_ub_property(property),
                                    data);
}

/* Please see header for spec */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void ogl_program_ub_sync(ogl_program_ub ub)
{
    ogl_program_block_sync( (ogl_program_block) ub);
}
