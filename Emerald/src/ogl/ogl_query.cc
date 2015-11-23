/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_query.h"
#include "ogl/ogl_rendering_handler.h"
#include "ral/ral_context.h"
#include "system/system_log.h"


/** TODO */
typedef struct _ogl_query_item
{
    GLuint gl_id;
    bool   has_started;

    _ogl_query_item()
    {
        gl_id       = 0;
        has_started = false;
    }

    ~_ogl_query_item()
    {
        ASSERT_DEBUG_SYNC(gl_id == 0,
                          "Sanity check failed.");
    }
} _ogl_query_item;

typedef struct _ogl_query
{
    /* DO NOT retain/release */
    raGL_backend     backend;
    ral_context      context;
    unsigned int     ring_buffer_size;
    _ogl_query_item* qo_items;
    GLenum           target_gl;

    /* Points at the next QO that can be peeked OR result of whose can be retrieved.
     * A value of -1 indicates no ogl_query_begin() call was made for this query object.
     *
     * index_peek must always be < ring_buffer_size.
     * index_peek must always be != index_next */
    int index_peek;

    /* Points at the QO which has been used for the last ogl_query_begin() call.
     *
     * It is an error if index_next == index_peek, and indicates the ring buffer has run
     * out of space. This can only occur if the query object is used for many queries in a row
     * without checking the queries' results in-between.
     *
     * index_next must always be < ring_buffer_size.
     */
    int index_current;

    /* Cached entry-points */
    PFNGLBEGINQUERYPROC          pGLBeginQuery;
    PFNGLDELETEQUERIESPROC       pGLDeleteQueries;
    PFNGLENDQUERYPROC            pGLEndQuery;
    PFNGLGENQUERIESPROC          pGLGenQueries;
    PFNGLGETQUERYOBJECTUIVPROC   pGLGetQueryObjectuiv;
    PFNGLGETQUERYOBJECTUI64VPROC pGLGetQueryObjectui64v;

     _ogl_query(ral_context  in_context,
                unsigned int in_ring_buffer_size,
                GLenum       in_target_gl);
    ~_ogl_query();
} _ogl_query;


/* Forward declarations */
PRIVATE void _ogl_query_deinit_renderer_callback(ogl_context context,
                                                 void*       user_arg);
PRIVATE void _ogl_query_init_renderer_callback  (ogl_context context,
                                                 void*       user_arg);


/** TODO */
_ogl_query::_ogl_query(ral_context  in_context,
                       unsigned int in_ring_buffer_size,
                       GLenum       in_target_gl)
{
    ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

    ASSERT_DEBUG_SYNC(in_ring_buffer_size > 0,
                      "Ring buffer size is invalid");

    context          = in_context;
    index_current    = -1;
    index_peek       = -1;
    qo_items         = new (std::nothrow) _ogl_query_item[in_ring_buffer_size];
    ring_buffer_size = in_ring_buffer_size;
    target_gl        = in_target_gl;

    ASSERT_DEBUG_SYNC(qo_items != NULL,
                      "Out of memory");

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND,
                            &backend);

    /* Cache ES/GL entry-points */
    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    if (backend_type == RAL_BACKEND_TYPE_GL)
    {
        const ogl_context_gl_entrypoints* entry_points = NULL;

        ogl_context_get_property(ral_context_get_gl_context(context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        pGLBeginQuery          = entry_points->pGLBeginQuery;
        pGLDeleteQueries       = entry_points->pGLDeleteQueries;
        pGLEndQuery            = entry_points->pGLEndQuery;
        pGLGenQueries          = entry_points->pGLGenQueries;
        pGLGetQueryObjectuiv   = NULL; /* not needed, we're using the 64-bit version instead */
        pGLGetQueryObjectui64v = entry_points->pGLGetQueryObjectui64v;
    } /* if (context_type == OGL_CONTEXT_TYPE_GL) */
    else
    {
        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_ES,
                          "Unrecognized rendering backend type");

        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(ral_context_get_gl_context(context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        pGLBeginQuery          = entry_points->pGLBeginQuery;
        pGLDeleteQueries       = entry_points->pGLDeleteQueries;
        pGLEndQuery            = entry_points->pGLEndQuery;
        pGLGenQueries          = entry_points->pGLGenQueries;
        pGLGetQueryObjectuiv   = entry_points->pGLGetQueryObjectuiv;
        pGLGetQueryObjectui64v = NULL;
    }

    /* Initialize the object */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                     _ogl_query_init_renderer_callback,
                                                     this);

}

/** TODO */
_ogl_query::~_ogl_query()
{
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                     _ogl_query_deinit_renderer_callback,
                                                     this);

    if (qo_items != NULL)
    {
        delete [] qo_items;

        qo_items = NULL;
    } /* if (qo_ids != NULL) */
}


/** TODO */
PRIVATE void _ogl_query_deinit_renderer_callback(ogl_context context,
                                                 void*       user_arg)
{
    _ogl_query* query_ptr = (_ogl_query*) user_arg;

    for (unsigned int n_item = 0;
                      n_item < query_ptr->ring_buffer_size;
                    ++n_item)
    {
        query_ptr->pGLDeleteQueries(1, /* n */
                                   &query_ptr->qo_items[n_item].gl_id);

        query_ptr->qo_items[n_item].gl_id = 0;
    } /* for (all items) */
}

/** TODO */
PRIVATE void _ogl_query_init_renderer_callback(ogl_context context,
                                               void*       user_arg)
{
    _ogl_query* query_ptr = (_ogl_query*) user_arg;

    for (unsigned int n_item = 0;
                      n_item < query_ptr->ring_buffer_size;
                    ++n_item)
    {
        query_ptr->pGLGenQueries(1, /* n */
                                &query_ptr->qo_items[n_item].gl_id);
    } /* for (all items) */
}


/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_query_begin(ogl_query query)
{
    _ogl_query* query_ptr = (_ogl_query*) query;

    /* Increment the 'current QO' index */
    query_ptr->index_current = (query_ptr->index_current + 1) % query_ptr->ring_buffer_size;

    ASSERT_DEBUG_SYNC(query_ptr->index_current != query_ptr->index_peek,
                      "Sanity check failed.");

    if (query_ptr->index_current != query_ptr->index_peek)
    {
        /* Kick off the query */
        query_ptr->pGLBeginQuery(query_ptr->target_gl,
                                 query_ptr->qo_items[query_ptr->index_current].gl_id);

        query_ptr->qo_items[query_ptr->index_current].has_started = true;
    }
    else
    {
        LOG_ERROR("ogl_query_begin(): Ignored, peek index == next index! Fetch query object results first.");
    }
}

/** Please see header for spec */
PUBLIC ogl_query ogl_query_create(ral_context  context,
                                  unsigned int ring_buffer_size,
                                  GLenum       gl_query_target)
{
    _ogl_query* new_instance = new (std::nothrow) _ogl_query(context,
                                                             ring_buffer_size,
                                                             gl_query_target);

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    return (ogl_query) new_instance;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_query_end(ogl_query query)
{
    _ogl_query*        query_ptr = (_ogl_query*) query;
    const unsigned int qo_id     = query_ptr->index_current;

    if (query_ptr->qo_items[qo_id].has_started)
    {
        /* Stop the query */
        query_ptr->pGLEndQuery(query_ptr->target_gl);

        query_ptr->qo_items[qo_id].has_started = false;
    }
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL bool ogl_query_peek_result(ogl_query query,
                                                         GLuint64* out_result)
{
    _ogl_query* query_ptr = (_ogl_query*) query;
    bool        result    = false;

    /* Sanity checks */
    const unsigned int index_peek = (query_ptr->index_peek + 1) % query_ptr->ring_buffer_size;

    ASSERT_DEBUG_SYNC(!query_ptr->qo_items[index_peek].has_started,
                      "The QO that is being peeked has not been ended yet.");

    /* Do we have any spare QO to use for the next begin operation? */
    bool should_force = ( (query_ptr->index_current + 1) % query_ptr->ring_buffer_size == query_ptr->index_peek);

    /* ES offers glGetQueryObjectuiv(), whereas under OpenGL we have glGetQueryObjectui64v() */
    if (query_ptr->pGLGetQueryObjectui64v != NULL)
    {
        /* GL code-path */
        GLuint64 temp = -1;

        query_ptr->pGLGetQueryObjectui64v(query_ptr->qo_items[index_peek].gl_id,
                                          GL_QUERY_RESULT_NO_WAIT,
                                         &temp);

        if (temp == -1 && should_force)
        {
            /* Got to stall the pipeline .. */
            static system_time last_warning_time = 0;
            static system_time one_sec_time      = system_time_get_time_for_s(1);
            static system_time time_now          = system_time_now();

            if (time_now - last_warning_time > one_sec_time)
            {
                LOG_ERROR("Performance warning: ogl_query_peek_result() about to force an implicit finish.");

                last_warning_time = time_now;
            }

            query_ptr->pGLGetQueryObjectui64v(query_ptr->qo_items[index_peek].gl_id,
                                              GL_QUERY_RESULT,
                                             &temp);
        }

        if (temp != -1)
        {
            /* The result has been returned - yay! */
            *out_result = temp;
            result      = true;

            /* Update the index */
            query_ptr->index_peek = index_peek;
        }
    } /* if (query_ptr->pGLGetQueryObjectui64v != NULL) */
    else
    {
        /* ES code-path */
        GLuint is_result_available = GL_FALSE;

        query_ptr->pGLGetQueryObjectuiv(query_ptr->qo_items[index_peek].gl_id,
                                        GL_QUERY_RESULT_AVAILABLE,
                                       &is_result_available);

        if (is_result_available == GL_TRUE ||
            should_force)
        {
            GLuint temp = -1;

            query_ptr->pGLGetQueryObjectuiv(query_ptr->qo_items[index_peek].gl_id,
                                            GL_QUERY_RESULT,
                                           &temp);

            *out_result = (GLuint64) temp;
             result     = true;

            /* Update the index */
            query_ptr->index_peek = index_peek;
        } /* if (is_result_available || should_force) */
    }

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_query_release(ogl_query query)
{
    _ogl_query* query_ptr = (_ogl_query*) query;

    /* Done */
    delete query_ptr;

    query_ptr = NULL;
}

