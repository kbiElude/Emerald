/**
 *
 * Emerald (kbi/elude @2015)
 *
 * ogl_programs acts as a context-wide storage for created programs.
 *
 * It caches all created programs by their names, thanks to call-backs
 * coming from ogl_program_create().
 *
 * The implementation itself is more than just straightforward, but the
 * separate module serves merely as an abstraction layer, rather than to
 * hide a complex logic of any kind.
 */
#ifndef OGL_PROGRAMS_H
#define OGL_PROGRAMS_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC ogl_programs ogl_programs_create();

/** TODO
 *
 *  Internal usage only.
 **/
PUBLIC ogl_program ogl_programs_get_program_by_id(__in __notnull ogl_programs programs,
                                                  __in           GLuint       po_id);

/** TODO
 *
 *  Internal usage only.
 **/
PUBLIC ogl_program ogl_programs_get_program_by_name(__in __notnull ogl_programs              programs,
                                                    __in __notnull system_hashed_ansi_string program_has);

/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC void ogl_programs_release(__in __notnull ogl_programs programs);

/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC void ogl_programs_register_program(__in __notnull ogl_programs programs,
                                          __in __notnull ogl_program  program);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC void ogl_programs_unregister_program(__in __notnull ogl_programs programs,
                                            __in __notnull ogl_program  program);


#endif /* OGL_PROGRAMS_H */
