/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Helps cache program object uniform setter calls. Note that these functions are private only.
 */
#ifndef OGL_QUERY_H
#define OGL_QUERY_H

#include "ogl/ogl_types.h"


/* Declare private handle */
DECLARE_HANDLE(ogl_query);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_query_begin(__in __notnull ogl_query query);

/** TODO */
PUBLIC ogl_query ogl_query_create(__in __notnull ogl_context  context,
                                  __in           unsigned int ring_buffer_size,
                                  __in           GLenum       gl_query_target);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_query_end(__in __notnull ogl_query query);

/** TODO.
 *
 *  This function can fail (that is: return false) if there are still QOs available in the ring buffer,
 *  and the results of ongoing queries are unavailable at the time of the call.
 *  It should, however, not fail if all the query objects, stored in the ring buffer, are busy. In
 *  such case, the oldest query object will be waited upon and the function will not quit, until the
 *  results become available.
 *
 **/
PUBLIC RENDERING_CONTEXT_CALL bool ogl_query_peek_result(__in      __notnull ogl_query query,
                                                         __out_opt           GLuint64* out_result);

/** TODO */
PUBLIC void ogl_query_release(__in __notnull __post_invalid ogl_query query);


#endif /* OGL_QUERY_H */
