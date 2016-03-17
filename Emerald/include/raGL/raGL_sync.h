#ifndef RAGL_SYNC_H
#define RAGL_SYNC_H

#include "raGL/raGL_types.h"


typedef enum
{
    /* not settable; GLsync */
    RAGL_SYNC_PROPERTY_HANDLE,

    /* not settable; raGL_backend */
    RAGL_SYNC_PROPERTY_PARENT_BACKEND,
} raGL_sync_property;


REFCOUNT_INSERT_DECLARATIONS(raGL_sync,
                             raGL_sync)


/** TODO */
PUBLIC RENDERING_CONTEXT_CALL raGL_sync raGL_sync_create();

/** TODO */
PUBLIC void raGL_sync_get_property(raGL_sync          sync,
                                   raGL_sync_property property,
                                   void*              out_result_ptr);

/** Issues a glWaitSync() call for the specified raGL_sync object, for the bound rendering
 *  context. This will block the rendering context on the GPU side until the sync object is
 *  executed elsewhere.
 *
 *  @param sync Sync object to use. Must not be NULL.
 **/
PUBLIC RENDERING_CONTEXT_CALL void raGL_sync_wait_gpu(raGL_sync sync);

#endif /* RAGL_SYNC_H */