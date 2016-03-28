/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_sync.h"
#include "system/system_log.h"


typedef struct _raGL_sync
{
    raGL_backend parent_backend;
    GLsync       sync;

    REFCOUNT_INSERT_VARIABLES

    explicit _raGL_sync(raGL_backend in_parent_backend,
                        GLsync       in_sync)
    {
        parent_backend = in_parent_backend;
        sync           = in_sync;
    }
} _raGL_sync;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(raGL_sync,
                               raGL_sync,
                              _raGL_sync);


/** Please see header for spec */
PRIVATE RENDERING_CONTEXT_CALL void _raGL_sync_release(void* sync)
{
    ogl_context                       current_context = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    bool                              is_nv_driver    = false;
    _raGL_sync*                       sync_ptr        = (_raGL_sync*) sync;

    ASSERT_DEBUG_SYNC(current_context != NULL,
                      "No rendering context bound to the calling thread");

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* NOTE: There appears to be a bug in NV driver where issuing a wait op against a fence sync does not increase
     *       the ref counter of the sync object, later to be decremented after the recorded commands finish executing.
     *       This, in turn, causes:
     *
     *       a) Hang-ups if glWaitSync() is issued in raGL_sync_wait_gpu() :( That's why we issue glClientWaitSync()
     *          ops there for NV platforms.
     *       b) Visual glitches if glClientWaitSync() is used instead.
     *
     *       Since none of the two is acceptable, we need to leak the handle :/
     *
     *       Does not affect the AMD driver.
     **/
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_IS_NV_DRIVER,
                            &is_nv_driver);

    if (!is_nv_driver)
    {
        entrypoints_ptr->pGLDeleteSync(sync_ptr->sync);
    }

    sync_ptr = nullptr;
}


/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL raGL_sync raGL_sync_create()
{
    ogl_context                       context_gl      = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    GLsync                            new_handle      = NULL;
    _raGL_sync*                       new_sync_ptr    = NULL;
    raGL_backend                      parent_backend  = NULL;

    /* Spawn a new sync object */
    ASSERT_DEBUG_SYNC(context_gl != NULL,
                      "No rendering context bound to the calling thread");

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_BACKEND,
                            &parent_backend);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    new_handle = entrypoints_ptr->pGLFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,
                                               0 /* flags */);

    ASSERT_DEBUG_SYNC(new_handle != NULL,
                      "GL returned a NULL GLsync object");

    entrypoints_ptr->pGLFlush();

    new_sync_ptr = new (std::nothrow) _raGL_sync(parent_backend,
                                                 new_handle);

    ASSERT_ALWAYS_SYNC(new_sync_ptr != NULL,
                       "Out of memory");

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_sync_ptr,
                                                   _raGL_sync_release,
                                                   OBJECT_TYPE_RAGL_SYNC,
                                                   NULL);

    return (raGL_sync) new_sync_ptr;
}

/** Please see header for spec */
PUBLIC void raGL_sync_get_property(raGL_sync          sync,
                                   raGL_sync_property property,
                                   void*              out_result_ptr)
{
    _raGL_sync* sync_ptr = (_raGL_sync*) sync;

    switch (property)
    {
        case RAGL_SYNC_PROPERTY_HANDLE:
        {
            *(GLsync*) out_result_ptr = sync_ptr->sync;

            break;
        }

        case RAGL_SYNC_PROPERTY_PARENT_BACKEND:
        {
            *(raGL_backend*) out_result_ptr = sync_ptr->parent_backend;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_sync_property value.");
        }
    }
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void raGL_sync_wait_gpu(raGL_sync sync)
{
    ogl_context                       current_context = ogl_context_get_current_context();
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    bool                              is_nv_driver    = false;
    _raGL_sync*                       sync_ptr        = (_raGL_sync*) sync;

    ASSERT_DEBUG_SYNC(current_context != NULL,
                      "No rendering context bound to the calling thread");

    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(current_context,
                             OGL_CONTEXT_PROPERTY_IS_NV_DRIVER,
                            &is_nv_driver);

    /* NOTE: See _raGL_sync_release() for more details about the workaround below. */
    if (is_nv_driver)
    {
        entrypoints_ptr->pGLClientWaitSync(sync_ptr->sync,
                                           GL_SYNC_FLUSH_COMMANDS_BIT,
                                          ~0);
    }
    else
    {
        entrypoints_ptr->pGLWaitSync(sync_ptr->sync,
                                     0, /* flags */
                                     GL_TIMEOUT_IGNORED);
    }
}
