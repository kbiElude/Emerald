/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef SH_PROJECTOR_COMBINER_H
#define SH_PROJECTOR_COMBINER_H

#include "shared.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(sh_projector_combiner, sh_projector_combiner)

/* Public type definitions */
typedef void     (*PFNPROJECTORCOMBINERCALLBACKPROC)(void*);
typedef uint32_t sh_projection_id;

typedef enum
{
    SH_PROJECTOR_COMBINER_CALLBACK_TYPE_ON_PROJECTION_ADDED,
    SH_PROJECTOR_COMBINER_CALLBACK_TYPE_ON_PROJECTION_DELETED
} _sh_projector_combiner_callback_type;

/** TODO */
PUBLIC EMERALD_API bool sh_projector_combiner_add_sh_projection(__in  __notnull   sh_projector_combiner,
                                                                __in              sh_projection_type,
                                                                __out __maybenull sh_projection_id*);

/** TODO. */
PUBLIC EMERALD_API sh_projector_combiner sh_projector_combiner_create(__in __notnull ogl_context               context,
                                                                      __in           sh_components             components,
                                                                      __in __notnull sh_samples                samples, 
                                                                      __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool sh_projector_combiner_delete_sh_projection(__in __notnull sh_projector_combiner,
                                                                   __in __notnull sh_projection_id);

/** TODO */
PUBLIC EMERALD_API bool sh_projector_combiner_execute(__in __notnull sh_projector_combiner instance,
                                                      __in           system_timeline_time  time
                                                     );

/** TODO */
PUBLIC EMERALD_API uint32_t sh_projector_combiner_get_amount_of_sh_projections(__in __notnull sh_projector_combiner);

/** TODO */
PUBLIC EMERALD_API bool sh_projector_combiner_get_sh_projection_property_details(__in __notnull sh_projector_combiner,
                                                                                 __in           sh_projection_id,
                                                                                 __in           sh_projector_property,
                                                                                 __out          void*);

/** TODO */
PUBLIC EMERALD_API bool sh_projector_combiner_get_sh_projection_property_id(__in  __notnull sh_projector_combiner,
                                                                            __in            uint32_t              index,
                                                                            __out __notnull sh_projection_id*     result);

/** TODO */
PUBLIC EMERALD_API void sh_projector_combiner_get_sh_projection_result_bo_data_details(__in  __notnull   sh_projector_combiner instance,
                                                                                       __out __maybenull GLuint*               result_bo_id,
                                                                                       __out __maybenull GLuint*               result_bo_offset,
                                                                                       __out __maybenull GLuint*               result_bo_data_size,
                                                                                       __out __maybenull ogl_texture*          result_tbo);

/** TODO */
PUBLIC EMERALD_API void sh_projector_combiner_set_callback(__in           _sh_projector_combiner_callback_type,
                                                           __in __notnull PFNPROJECTORCOMBINERCALLBACKPROC,
                                                           __in __notnull void*);

#endif /* SH_PROJECTOR_COMBINER_H */