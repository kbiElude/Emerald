/**
 *
 * Emerald (kbi/elude @2012)
 * 
 *
 */
#ifndef SH_PROJECTOR_H
#define SH_PROJECTOR_H

#include "shared.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(sh_projector, sh_projector)

typedef enum
{
    SH_PROJECTION_RESULT_RRGGBB,
    SH_PROJECTION_RESULT_RGBRGB
} _sh_projection_result;

/** TODO */
PUBLIC EMERALD_API sh_projector sh_projector_create(__in __notnull ogl_context               context,
                                                    __in __notnull sh_samples                samples,
                                                    __in           sh_components             components,
                                                    __in __notnull system_hashed_ansi_string name,
                                                    __in           uint32_t                  n_projections,
                                                    __in __notnull const sh_projection_type* projections);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void sh_projector_execute(__in           __notnull   sh_projector,
                                                                    __in_ecount(3) __notnull   float*       rotation_angles_radians,
                                                                    __in           __notnull   GLuint       result_bo_id,     /* result buffer object id */
                                                                    __in           __notnull   GLuint       result_bo_offset, /* result buffer's start offset */
                                                                    __in           __maybenull float*       result_ptr);      /* place to store the results in user-space. can be null - faster! */

/** TODO */
PUBLIC EMERALD_API sh_components sh_projector_get_components(__in __notnull sh_projector);

/** TODO */
PUBLIC EMERALD_API void sh_projector_get_projection_result_details(__in  __notnull   sh_projector          instance,
                                                                   __in              _sh_projection_result type,
                                                                   __out __maybenull uint32_t*             result_bo_offset,
                                                                   __out __maybenull uint32_t*             result_bo_size);

/** TODO */
PUBLIC EMERALD_API uint32_t sh_projector_get_projection_result_total_size(__in __notnull sh_projector instance);

/** TODO */
PUBLIC EMERALD_API bool sh_projector_set_projection_property(__in __notnull sh_projector          instance,
                                                             __in           uint32_t              n_projection,
                                                             __in           sh_projector_property property,
                                                             __in           const void*           data);

#endif /* SH_PROJECTOR_H */