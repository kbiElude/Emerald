/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SCALAR_FIELD_METABALLS_H
#define SCALAR_FIELD_METABALLS_H

#include "ogl/ogl_types.h"

DECLARE_HANDLE(scalar_field_metaballs);

REFCOUNT_INSERT_DECLARATIONS(scalar_field_metaballs,
                             scalar_field_metaballs)


typedef enum
{
    /* GLuint; not settable */
    SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_ID,

    /* unsigned int; not settable */
    SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_SIZE,

    /* unsigned int; not settable */
    SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_START_OFFSET
} scalar_field_metaballs_property;


/** TODO */
PUBLIC EMERALD_API scalar_field_metaballs scalar_field_metaballs_create(ogl_context               context,
                                                                        const unsigned int*       grid_size_xyz,
                                                                        system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void scalar_field_metaballs_get_property(scalar_field_metaballs          metaballs,
                                                            scalar_field_metaballs_property property,
                                                            void*                           out_result);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void scalar_field_metaballs_update(scalar_field_metaballs metaballs);

#endif /* SCALAR_FIELD_METABALLS_H */
